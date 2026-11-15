#pragma once

#include <QList>
#include <QString>

// Локализация на простых JSON-файлах: {"_name": "Русский", "English text": "Перевод", ...}.
// Исходные строки в коде английские, поэтому для English файл не нужен.
// Файлы ищутся в <корень Cask>/lang/<код>.json (приоритет) и во встроенных ресурсах :/lang/.
namespace I18n {

struct Language {
    QString code;  // "en", "ru"
    QString name;  // название на самом языке
};

QList<Language> available();           // English всегда первый
QString systemLanguage();              // код, подобранный по языку системы (или "en")
QString resolve(const QString& setting);  // "system" -> код, неизвестный код -> "en"
QString currentCode();
bool apply(const QString& setting);    // "system" или код языка; false, если файл не загрузился

}  // namespace I18n
