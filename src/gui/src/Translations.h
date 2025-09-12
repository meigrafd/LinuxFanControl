#pragma once
#include <QString>
#include <QHash>
#include <QVariant>

/* Filesystem-based i18n loader.
 * Locale directory list:
 *  - env LFCD_LOCALES
 *  - <binary_dir>/locales
 *  - <source_dir>/locales  (pass on construction)
 *
 * Looks for: locales/<lang>/messages.json or locales/<lang>.json
 * Default language is "en".
 */
class Translations {
public:
    explicit Translations(QString sourceDir = {});
    void setLanguage(const QString& lang);
    QString language() const { return lang_; }
    QString t(const QString& key, const QVariantMap& args = {}) const;

private:
    QString findLocaleFile(const QString& lang) const;
    void load(const QString& lang);

    QString lang_ = "en";
    QHash<QString, QString> map_;
    QString sourceDir_;
};
