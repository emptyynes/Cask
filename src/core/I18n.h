#pragma once

#include <QList>
#include <QString>

// Localization via plain JSON files: {"_name": "Русский", "English text": "Translation", ...}.
// Source strings in code are English, so English needs no file.
// Files are looked up in <Cask root>/lang/<code>.json (takes priority) and in the embedded resources :/lang/.
namespace I18n {

struct Language {
    QString code;  // "en", "ru"
    QString name;  // name in the language itself
};

QList<Language> available();           // English is always first
QString systemLanguage();              // code matched to the system language (or "en")
QString resolve(const QString& setting);  // "system" -> code, unknown code -> "en"
QString currentCode();
bool apply(const QString& setting);    // "system" or a language code; false if the file failed to load

}  // namespace I18n
