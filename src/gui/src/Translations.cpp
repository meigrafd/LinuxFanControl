#include "Translations.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

Translations& Translations::instance() {
    static Translations inst;
    return inst;
}

Translations::Translations(QObject* parent)
: QObject(parent) {}

QString Translations::localesDir() const {
    // build step may copy locales/ next to the binary
    const QString base = QCoreApplication::applicationDirPath() + "/locales";
    return base;
}

QStringList Translations::availableLanguages() const {
    QStringList out;
    QDir dir(localesDir());
    if (!dir.exists()) return out;

    const QStringList files = dir.entryList(QStringList() << "*.qm", QDir::Files);
    for (const auto& f : files) {
        // Accept: xx.qm | gui_xx.qm | lfc_xx.qm
        QString code;
        if (f.endsWith(".qm")) {
            const QString base = f.left(f.size()-3);
            const int us = base.lastIndexOf('_');
            if (us >= 0)
                code = base.mid(us+1);
            else
                code = base;
        }
        if (!code.isEmpty() && !out.contains(code))
            out << code;
    }
    out.sort();
    return out;
}

bool Translations::setLanguage(const QString& code) {
    // Remove old translator first (Qt auto-handles duplicates)
    if (!current_.isEmpty())
        qApp->removeTranslator(&translator_);

    const QString dir = localesDir();

    const QString try1 = dir + "/" + code + ".qm";
    const QString try2 = dir + "/gui_" + code + ".qm";
    const QString try3 = dir + "/lfc_" + code + ".qm";

    bool ok =
    translator_.load(try1) ||
    translator_.load(try2) ||
    translator_.load(try3);

    if (!ok) {
        current_.clear();
        return false;
    }

    qApp->installTranslator(&translator_);
    current_ = code;
    // notify widgets to retranslate in next paint/event pass
    QEvent ev(QEvent::LanguageChange);
    qApp->sendEvent(qApp, &ev);
    return true;
}
