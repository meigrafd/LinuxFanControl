/*
 * GUI side JSON-RPC client (spawns daemon) for lfc-gui.
 * (c) 2025 meigrafd & contributors - MIT
 *
 * Fix: Avoid GUI freeze by replacing QWaitCondition blocking with
 *       an event-loop driven wait that pumps Qt events until responses arrive.
 */

#include "RpcClient.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcessEnvironment>
#include <QElapsedTimer>
#include <QDir>
#include <QDateTime>
#include <QEventLoop>
#include <QTimer>
#include <QThread>

static inline QString nowIso() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

RpcClient::RpcClient(QObject* parent) : QObject(parent) {
    debug_ = qEnvironmentVariableIntValue("LFC_GUI_DEBUG") != 0;
}

RpcClient::~RpcClient() {
    if (proc_) {
        proc_->disconnect(this);
        proc_->kill();
        proc_->waitForFinished(1500);
        delete proc_;
        proc_ = nullptr;
    }
}

bool RpcClient::isRunning() const {
    return proc_ && proc_->state() == QProcess::Running;
}

QString RpcClient::shellQuote(const QString& s) {
    // POSIX single-quote escaping: ' -> '"'"'
    QString out("'");
    for (QChar c : s) {
        if (c == '\'') out += "'\"'\"'";
        else out += c;
    }
    out += "'";
    return out;
}

bool RpcClient::launchDaemon(QString* err) {
    if (proc_) { proc_->deleteLater(); proc_ = nullptr; }

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString daemon  = env.value("LFC_DAEMON", "./build/lfcd");
    const QString args    = env.value("LFC_DAEMON_ARGS", "--debug");
    const QString wrapper = env.value("LFC_DAEMON_WRAPPER");
    const QString workdir = QDir::currentPath();

    QString program;
    QStringList arguments;

    if (!wrapper.trimmed().isEmpty()) {
        program = "/bin/sh";
        const QString cmd = wrapper + " " + shellQuote(daemon) + " " + args;
        arguments << "-c" << cmd;
    } else {
        program = daemon;
        arguments = QProcess::splitCommand(args);
    }

    proc_ = new QProcess(this);
    proc_->setProgram(program);
    proc_->setArguments(arguments);
    proc_->setWorkingDirectory(workdir);
    proc_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(proc_, &QProcess::readyReadStandardOutput, this, &RpcClient::onReadyRead);
    connect(proc_, &QProcess::readyReadStandardError,  this, &RpcClient::onReadyRead);
    connect(proc_, &QProcess::errorOccurred,           this, &RpcClient::onErrorOccurred);
    connect(proc_, qOverload<int,QProcess::ExitStatus>(&QProcess::finished),
            this, &RpcClient::onFinished);

    if (debug_) emit daemonLogLine(QString("[%1] [rpc] spawn: %2 %3")
        .arg(nowIso(), program, arguments.join(' ')));
    proc_->start();
    if (!proc_->waitForStarted(3000)) {
        if (err) *err = QString("failed to start daemon: %1").arg(proc_->errorString());
        proc_->deleteLater(); proc_ = nullptr;
        return false;
    }
    return true;
}

bool RpcClient::ensureRunning(QString* err) {
    if (isRunning()) return true;
    return launchDaemon(err);
}

QString RpcClient::nextId() { return QString::number(seq_++); }

QPair<QJsonValue, QString> RpcClient::call(const QString& method,
                                           const QJsonObject& params,
                                           int timeoutMs) {
    QString err;
    if (!ensureRunning(&err)) return {QJsonValue(), err};
    const QString id = nextId();

    QJsonObject env;
    env["jsonrpc"] = "2.0";
    env["id"]      = id;
    env["method"]  = method;
    env["params"]  = params;

    if (!sendJsonLine(env)) return {QJsonValue(), "I/O write failed"};

    QJsonValue out;
    if (!waitForId(id, &out, &err, timeoutMs))
        return {QJsonValue(), err.isEmpty() ? QStringLiteral("timeout") : err};
    return {out, QString()};
                                           }

                                           QPair<QJsonArray, QString> RpcClient::callBatch(const QJsonArray& batch, int timeoutMs) {
                                               QString err;
                                               if (!ensureRunning(&err)) return {QJsonArray(), err};

                                               QJsonArray outBatch;
                                               QSet<QString> ids;
                                               for (const QJsonValue& v : batch) {
                                                   if (!v.isObject()) continue;
                                                   QJsonObject o = v.toObject();
                                                   o["jsonrpc"] = "2.0";
                                                   if (!o.contains("id")) o["id"] = nextId();
                                                   ids.insert(o["id"].toString());
                                                   outBatch.push_back(o);
                                               }

                                               QJsonDocument doc(outBatch);
                                               if (!sendJsonLineRaw(doc)) return {QJsonArray(), "I/O write failed"};

                                               QJsonArray resp;
                                               if (!waitForBatchIds(ids, &resp, &err, timeoutMs))
                                                   return {QJsonArray(), err.isEmpty() ? QStringLiteral("timeout") : err};
                                               return {resp, QString()};
                                           }

                                           bool RpcClient::sendJsonLine(const QJsonObject& obj) {
                                               QJsonDocument doc(obj);
                                               return sendJsonLineRaw(doc);
                                           }
                                           bool RpcClient::sendJsonLineRaw(const QJsonDocument& doc) {
                                               if (!proc_) return false;
                                               QByteArray line = doc.toJson(QJsonDocument::Compact);
                                               line.append('\n');
                                               return proc_->write(line) == line.size();
                                           }

                                           void RpcClient::onReadyRead() {
                                               if (!proc_) return;
                                               buf_.append(proc_->readAllStandardOutput());
                                               buf_.append(proc_->readAllStandardError());

                                               while (true) {
                                                   int nl = buf_.indexOf('\n');
                                                   if (nl < 0) break;
                                                   QByteArray one = buf_.left(nl);
                                                   buf_.remove(0, nl + 1);
                                                   const QString s = QString::fromUtf8(one).trimmed();
                                                   if (s.isEmpty()) continue;

                                                   QJsonParseError pe{};
                                                   QJsonDocument jd = QJsonDocument::fromJson(one, &pe);
                                                   if (pe.error == QJsonParseError::NoError && (jd.isObject() || jd.isArray())) {
                                                       if (jd.isObject()) {
                                                           const QJsonObject env = jd.object();
                                                           const QString id = env.value("id").toVariant().toString();
                                                           QMutexLocker lk(&mtx_);
                                                           if (env.contains("result") || env.contains("error")) {
                                                               pending_[id]    = env.value("result").isNull() ? env.value("error") : env.value("result");
                                                               pendingEnv_[id] = env;
                                                           }
                                                       } else {
                                                           const QJsonArray arr = jd.array();
                                                           QMutexLocker lk(&mtx_);
                                                           for (const QJsonValue& v : arr) {
                                                               if (!v.isObject()) continue;
                                                               const QJsonObject env = v.toObject();
                                                               const QString id = env.value("id").toVariant().toString();
                                                               pending_[id]    = env.value("result").isNull() ? env.value("error") : env.value("result");
                                                               pendingEnv_[id] = env;
                                                           }
                                                       }
                                                   } else {
                                                       emit daemonLogLine(s);
                                                   }
                                               }
                                           }

                                           void RpcClient::onErrorOccurred(QProcess::ProcessError e) {
                                               emit daemonLogLine(QString("[rpc] process error: %1").arg(static_cast<int>(e)));
                                           }
                                           void RpcClient::onFinished(int exitCode, QProcess::ExitStatus status) {
                                               emit daemonLogLine(QString("[rpc] daemon finished: code=%1, status=%2").arg(exitCode).arg(int(status)));
                                               emit daemonCrashed(exitCode, status);
                                           }

                                           bool RpcClient::waitForId(const QString& id, QJsonValue* out, QString* err, int timeoutMs) {
                                               QElapsedTimer timer; timer.start();
                                               while (true) {
                                                   {
                                                       QMutexLocker lk(&mtx_);
                                                       auto it = pending_.find(id);
                                                       if (it != pending_.end()) {
                                                           *out = it.value();
                                                           pending_.erase(it);
                                                           pendingEnv_.remove(id);
                                                           return true;
                                                       }
                                                   }
                                                   if (timeoutMs >= 0 && timer.elapsed() >= timeoutMs) {
                                                       if (err) { *err = "timeout"; }
                                                       return false;
                                                   }
                                                   if (proc_ && proc_->state() == QProcess::NotRunning) {
                                                       if (err) { *err = "daemon exited"; }
                                                       return false;
                                                   }
                                                   // Pump events for ~10ms so readyRead can fire on this thread.
                                                   QEventLoop loop;
                                                   QTimer::singleShot(10, &loop, &QEventLoop::quit);
                                                   loop.exec(QEventLoop::AllEvents);
                                               }
                                           }

                                           bool RpcClient::waitForBatchIds(const QSet<QString>& ids, QJsonArray* out, QString* err, int timeoutMs) {
                                               QElapsedTimer timer; timer.start();
                                               while (true) {
                                                   bool all = true;
                                                   {
                                                       QMutexLocker lk(&mtx_);
                                                       for (const QString& id : ids) {
                                                           if (!pendingEnv_.contains(id)) { all = false; break; }
                                                       }
                                                       if (all) {
                                                           QJsonArray arr;
                                                           for (const QString& id : ids) {
                                                               arr.push_back(pendingEnv_.take(id));
                                                               pending_.remove(id);
                                                           }
                                                           *out = arr;
                                                           return true;
                                                       }
                                                   }
                                                   if (timeoutMs >= 0 && timer.elapsed() >= timeoutMs) {
                                                       if (err) { *err = "timeout"; }
                                                       return false;
                                                   }
                                                   if (proc_ && proc_->state() == QProcess::NotRunning) {
                                                       if (err) { *err = "daemon exited"; }
                                                       return false;
                                                   }
                                                   QEventLoop loop;
                                                   QTimer::singleShot(10, &loop, &QEventLoop::quit);
                                                   loop.exec(QEventLoop::AllEvents);
                                               }
                                           }

                                           // ---- convenience wrappers ----
                                           QJsonArray RpcClient::listSensors(QString* err) {
                                               auto res = call(QStringLiteral("listSensors"));
                                               if (!res.second.isEmpty()) { if (err) *err = res.second; return {}; }
                                               return res.first.toArray();
                                           }
                                           QJsonArray RpcClient::listPwms(QString* err) {
                                               auto res = call(QStringLiteral("listPwms"));
                                               if (!res.second.isEmpty()) { if (err) *err = res.second; return {}; }
                                               return res.first.toArray();
                                           }
                                           QJsonObject RpcClient::enumerate(QString* err) {
                                               QJsonArray batch;
                                               batch.push_back(QJsonObject{{"method","listSensors"},{"params",QJsonObject{}}});
                                               batch.push_back(QJsonObject{{"method","listPwms"},{"params",QJsonObject{}}});
                                               auto res = callBatch(batch, 20000);
                                               if (!res.second.isEmpty()) { if (err) *err = res.second; return {}; }
                                               QJsonObject out;
                                               for (const auto& v : res.first) {
                                                   const auto o = v.toObject();
                                                   const auto r = o.value("result");
                                                   if (r.isArray() && !r.toArray().isEmpty() &&
                                                       r.toArray().first().isObject() &&
                                                       r.toArray().first().toObject().contains("pwm_path"))
                                                       out["pwms"] = r;
                                                   else if (r.isArray())
                                                       out["sensors"] = r;
                                               }
                                               if (!out.contains("sensors")) out["sensors"] = QJsonArray();
                                               if (!out.contains("pwms"))    out["pwms"] = QJsonArray();
                                               return out;
                                           }
                                           QJsonArray RpcClient::listChannels(QString* err) {
                                               auto res = call(QStringLiteral("listChannels"));
                                               if (!res.second.isEmpty()) { if (err) *err = res.second; return {}; }
                                               return res.first.toArray();
                                           }
