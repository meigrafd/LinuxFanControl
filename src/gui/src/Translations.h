#pragma once
// Translations helper: load/unload Qt .qm files at runtime.
// Comments in English; UI text language switch handled elsewhere.

#include <QObject>
#include <QTranslator>
#include <QString>
#include <QStringList>

class Translations : public QObject {
    Q_OBJECT
public:
    static Translations& instance();

    // Change UI language. Looks into "<appdir>/locales".
    // Tries: "<code>.qm", "gui_<code>.qm", "lfc_<code>.qm".
    bool setLanguage(const QString& code);

    QString currentLanguage() const { return current_; }

    // List codes discovered in "<appdir>/locales" (best-effort).
    QStringList availableLanguages() const;

private:
    explicit Translations(QObject* parent=nullptr);
    QString localesDir() const;

private:
    mutable QTranslator translator_;
    QString current_;
};
