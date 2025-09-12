/*
 * Linux Fan Control (LFC) - GUI RpcClient
 * (c) 2025 meigrafd & contributors - MIT License (see LICENSE)
 */

#include "RpcClient.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QTimer>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QDateTime>

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

bool RpcClient::ensureRunning(QString* err) {
    if (isRunning()) return true;
    return launchDaemon(err);
}

bool RpcClient::launchDaemon(QString* err) {
    if (proc_) {
        proc_->deleteLater();
        proc_ = nullptr;
    }

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString daemon = env.value("LFC_DAEMON", "./build/lfcd");
    const QString daemonArgs = env.value("LFC_DAEMON_ARGS", "--debug");
    const QString wrapper = env.value("LFC_DAEMON_WRAPPER"); // e.g. "gdb -ex run --args" or "valgrind ..."
    const QString workdir = QDir::currentPath();

    QString program;
    QStringList arguments;

    if (!wrapper.trimmed().isEmpty()) {
        // Use a shell to run wrapper + daemon + args in one command line.
        program = "/bin/sh";
        QString cmd = wrapper + " " + QProcess::quote(daemon) + " " + daemonArgs;
        arguments << "-c" << cmd;
    } else {
        program = daemon;
        arguments = QProcess::splitCommand(daemonArgs);
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

    if (debug_) {
        emit daemonLogLine(QString("[%1] [rpc] spawn: %2 %3")
        .arg(nowIso(), program, arguments.join(' ')));
    }

    proc_->start();
    if (!proc_->waitForStarted(3000)) {
        if (err) *err = QString("failed to start daemon: %1").arg(proc_->errorString());
        proc_->deleteLater(); proc_ = nullptr;
        return false;
    }
    return true;
}

QString RpcClient::nextId() {
    return QString::number(seq_++);
}

QPair<QJsonValue, QString> RpcClient::call(const QString& method,
                                           const QJsonObject& params,
                                           int timeoutMs) {
    QString err;
    if (!ensureRunning(&err)) return {QJsonValue(), err};

    const QString id = nextId();

    QJsonObject env;
    env["jsonrpc"] = "2.0";
    env["id"] = id;
    env["method"] = method;
    env["params"] = params;

    if (!sendJsonLine(env)) {
        return {QJsonValue(), "I/O write failed"};
    }

    QJsonValue out;
    if (!waitForId(id, &out, &err, timeoutMs)) {
        return {QJsonValue(), err.isEmpty() ? QStringLiteral("timeout") : err};
    }
    return {out, QString()};
}

QPair<QJsonArray, QString> RpcClient::callBatch(const QJsonArray& batch, int timeoutMs) {
    QString err;
    if (!ensureRunning(&err)) return {QJsonArray(), err};

    // If caller sent non-ids, add ids now.
    QJsonArray outBatch;
    QSet<QString> ids;
    outBatch.reserve(batch.size());
    for (const QJsonValue& v : batch) {
        if (!v.isObject()) continue;
        QJsonObject o = v.toObject();
        o["jsonrpc"] = "2.0";
        if (!o.contains("id")) o["id"] = nextId();
        ids.insert(o["id"].toString());
        outBatch.push_back(o);
    }

    // Send as a single JSON array line
    QJsonDocument doc(outBatch);
    if (!sendJsonLineRaw(doc)) {
        return {QJsonArray(), "I/O write failed"};
    }

    QJsonArray resp;
    if (!waitForBatchIds(ids, &resp, &err, timeoutMs)) {
        return {QJsonArray(), err.isEmpty() ? QStringLiteral("timeout") : err};
    }
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
    const qint64 n = proc_->write(line);
    return (n == line.size());
}

void RpcClient::onReadyRead() {
    if (!proc_) return;
    buf_.append(proc_->readAllStandardOutput());
    buf_.append(proc_->readAllStandardError()); // optional: funnel stderr into log

    // emit log lines (non-JSON) to UI
    while (true) {
        int nl = buf_.indexOf('\n');
        if (nl < 0) break;
        QByteArray one = buf_.left(nl);
        buf_.remove(0, nl + 1);
        const QString s = QString::fromUtf8(one).trimmed();
        if (s.isEmpty()) continue;

        // Try parse as JSON; if it fails, it's a daemon log line.
        QJsonParseError pe{};
        QJsonDocument jd = QJsonDocument::fromJson(one, &pe);
        if (pe.error == QJsonParseError::NoError && (jd.isObject() || jd.isArray())) {
            // Route JSON envelope(s)
            if (jd.isObject()) {
                const QJsonObject env = jd.object();
                const QString id = env.value("id").toVariant().toString();
                QMutexLocker lk(&mtx_);
                if (env.contains("result") || env.contains("error")) {
                    pending_[id] = env.value("result").isNull() ? env.value("error") : env.value("result");
                    pendingEnv_[id] = env;
                    cond_.wakeAll();
                }
            } else if (jd.isArray()) {
                // Batch response: keep each item by id
                const QJsonArray arr = jd.array();
                QMutexLocker lk(&mtx_);
                for (const QJsonValue& v : arr) {
                    if (!v.isObject()) continue;
                    const QJsonObject env = v.toObject();
                    const QString id = env.value("id").toVariant().toString();
                    pending_[id] = env.value("result").isNull() ? env.value("error") : env.value("result");
                    pendingEnv_[id] = env;
                }
                cond_.wakeAll();
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
    emit daemonLogLine(QString("[rpc] daemon finished: code=%1, status=%2")
    .arg(exitCode).arg(int(status)));
    emit daemonCrashed(exitCode, status);
}

bool RpcClient::waitForId(const QString& id, QJsonValue* out, QString* err, int timeoutMs) {
    QElapsedTimer t; t.start();
    QMutexLocker lk(&mtx_);
    while (!pending_.contains(id)) {
        const qint64 left = timeoutMs < 0 ? 1000 : std::max<qint64>(1, timeoutMs - t.elapsed());
        if (!cond_.wait(&mtx_, left)) {
            if (timeoutMs >= 0 && t.elapsed() >= timeoutMs) {
                if (err) *err = "timeout";
                return false;
            }
        }
    }
    *out = pending_.take(id);
    pendingEnv_.remove(id);
    return true;
}

bool RpcClient::waitForBatchIds(const QSet<QString>& ids, QJsonArray* out, QString* err, int timeoutMs) {
    QElapsedTimer t; t.start();
    QMutexLocker lk(&mtx_);
    while (true) {
        bool all = true;
        for (const QString& id : ids) {
            if (!pendingEnv_.contains(id)) { all = false; break; }
        }
        if (all) break;
        const qint64 left = timeoutMs < 0 ? 1000 : std::max<qint64>(1, timeoutMs - t.elapsed());
        if (!cond_.wait(&mtx_, left)) {
            if (timeoutMs >= 0 && t.elapsed() >= timeoutMs) {
                if (err) *err = "timeout";
                return false;
            }
        }
    }
    QJsonArray arr;
    for (const QString& id : ids) {
        arr.push_back(pendingEnv_.take(id));
        pending_.remove(id);
    }
    *out = arr;
    return true;
}
