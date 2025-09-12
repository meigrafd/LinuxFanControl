#include "Translations.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

static QStringList localeDirs(const QString& srcDir) {
    QStringList dirs;
    if (qEnvironmentVariableIsSet("LFCD_LOCALES"))
        dirs << qEnvironmentVariable("LFCD_LOCALES");
    dirs << (QCoreApplication::applicationDirPath() + "/locales");
    if (!srcDir.isEmpty()) dirs << (srcDir + "/locales");
    return dirs;
}

Translations::Translations(QString sourceDir) : sourceDir_(std::move(sourceDir)) {
    load(lang_);
}

QString Translations::findLocaleFile(const QString& lang) const {
    const auto dirs = localeDirs(sourceDir_);
    for (const auto& d : dirs) {
        QString p1 = QDir(d).filePath(lang + "/messages.json");
        QString p2 = QDir(d).filePath(lang + ".json");
        if (QFile::exists(p1)) return p1;
        if (QFile::exists(p2)) return p2;
    }
    return {};
}

void Translations::load(const QString& lang) {
    map_.clear();
    QString path = findLocaleFile(lang);
    QString use = lang;
    if (path.isEmpty() && lang != "en") {
        path = findLocaleFile("en");
        use = "en";
    }
    if (!path.isEmpty()) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            auto doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                for (auto it = doc.object().begin(); it != doc.object().end(); ++it) {
                    map_.insert(it.key(), it.value().toString());
                }
            }
        }
    }
    lang_ = use;
}

void Translations::setLanguage(const QString& lang) { load(lang); }

QString Translations::t(const QString& key, const QVariantMap& args) const {
    QString s = map_.value(key, key);
    // naive {name} formatter
    for (auto it = args.begin(); it != args.end(); ++it) {
        s.replace("{" + it.key() + "}", it.value().toString());
    }
    return s;
}
