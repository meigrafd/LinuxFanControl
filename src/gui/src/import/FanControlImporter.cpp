#include "import/FanControlImporter.h"
#include "RpcClient.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace {

    QJsonObject enumerate(RpcClient* rpc) {
        auto res = rpc->enumerate();
        return res.contains("result") ? res["result"].toObject() : QJsonObject();
    }

    QString findBySubstr(const QJsonArray& arr, const QString& key, const QString& needle) {
        if (needle.isEmpty()) return {};
        for (const auto& v : arr) {
            auto o = v.toObject();
            const QString s = o.value(key).toString();
            if (s.contains(needle, Qt::CaseInsensitive)) return s;
        }
        return {};
    }

    QString findSensorPath(const QJsonArray& sensors, const QString& labelSubstr) {
        for (const auto& v : sensors) {
            auto o = v.toObject();
            const QString lab = o.value("label").toString();
            if (lab.contains(labelSubstr, Qt::CaseInsensitive)) {
                return o.value("path").toString();
            }
        }
        return {};
    }

    QString findPwmPath(const QJsonArray& pwms, const QString& labelSubstr) {
        for (const auto& v : pwms) {
            auto o = v.toObject();
            const QString lab = o.value("label").toString();
            if (lab.contains(labelSubstr, Qt::CaseInsensitive)) {
                return o.value("pwm").toString();
            }
        }
        return {};
    }

} // anon

namespace Importer {

    bool importFanControlJson(RpcClient* rpc, const QString& filePath, QString* errorOut) {
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly)) {
            if (errorOut) *errorOut = "Cannot open file";
            return false;
        }
        auto doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) { if (errorOut) *errorOut = "Invalid JSON"; return false; }
        auto root = doc.object();

        // Very generic schema: try to read array "Fans" with objects:
        // { "Name": "...", "PwmLabel": "...", "SensorLabel": "...", "Points": [ [x,y], ... ], "Hyst": 0.5, "Tau": 2.0, "Mode":"Auto|Manual", "ManualPct": 40 }
        // This is a best-effort mapper; users can fix labels afterwards.

        auto e = enumerate(rpc);
        const auto sensors = e.value("sensors").toArray();
        const auto pwms    = e.value("pwms").toArray();

        QJsonArray fans = root.value("Fans").toArray();
        if (fans.isEmpty()) {
            if (errorOut) *errorOut = "No 'Fans' array found. Please export a compatible JSON.";
            return false;
        }

        // Build batch requests
        QJsonArray batch;
        int idc = 1;

        for (const auto& v : fans) {
            const auto o = v.toObject();
            const QString name   = o.value("Name").toString();
            const QString pwmL   = o.value("PwmLabel").toString();
            const QString sensL  = o.value("SensorLabel").toString();
            const QJsonArray pts = o.value("Points").toArray();
            const double hyst    = o.value("Hyst").toDouble(0.0);
            const double tau     = o.value("Tau").toDouble(0.0);
            const QString mode   = o.value("Mode").toString("Auto");
            const double manual  = o.value("ManualPct").toDouble(0.0);

            const QString pwmPath   = findPwmPath(pwms, pwmL);
            const QString sensorPath= findSensorPath(sensors, sensL);
            if (pwmPath.isEmpty() || sensorPath.isEmpty()) continue;

            // 1) createChannel
            batch.push_back(QJsonObject{
                {"jsonrpc","2.0"},
                {"id", QString::number(idc++)},
                            {"method","createChannel"},
                            {"params", QJsonObject{{"name", name.isEmpty() ? pwmL : name},
                            {"sensor", sensorPath},
                            {"pwm", pwmPath}}}
            });

            // 2) set curve
            if (!pts.isEmpty()) {
                batch.push_back(QJsonObject{
                    {"jsonrpc","2.0"},
                    {"id", QString::number(idc++)},
                                {"method","setChannelCurve"},
                                {"params", QJsonObject{{"id", name.isEmpty() ? pwmL : name},
                                {"points", pts}}}
                });
            }

            // 3) hyst/tau
            batch.push_back(QJsonObject{
                {"jsonrpc","2.0"},
                {"id", QString::number(idc++)},
                            {"method","setChannelHystTau"},
                            {"params", QJsonObject{{"id", name.isEmpty() ? pwmL : name},
                            {"hyst", hyst},
                            {"tau", tau}}}
            });

            // 4) mode/manual
            batch.push_back(QJsonObject{
                {"jsonrpc","2.0"},
                {"id", QString::number(idc++)},
                            {"method","setChannelMode"},
                            {"params", QJsonObject{{"id", name.isEmpty() ? pwmL : name},
                            {"mode", mode}}}
            });
            if (mode.compare("Manual", Qt::CaseInsensitive) == 0) {
                batch.push_back(QJsonObject{
                    {"jsonrpc","2.0"},
                    {"id", QString::number(idc++)},
                                {"method","setChannelManual"},
                                {"params", QJsonObject{{"id", name.isEmpty() ? pwmL : name},
                                {"pct", manual}}}
                });
            }
        }

        if (!batch.isEmpty()) {
            rpc->callBatch(batch);
            rpc->call("engineStart");
        }
        return true;
    }

} // namespace Importer
