#pragma once

#include <QMap>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

// Настройки запуска, хранятся у каждого префикса отдельно (cask.json -> "settings").
// Синхронизацию (esync/fsync/ntsync) Cask не трогает: современный Wine сам выбирает ntsync.
struct PrefixSettings {
    QString locale;                          // пусто = язык системы, иначе например ru_RU.UTF-8
    bool gamemode = false;                   // запускать программы через gamemoderun
    QMap<QString, QString> env;              // свои переменные окружения

    bool operator==(const PrefixSettings& o) const
    {
        return locale == o.locale && gamemode == o.gamemode && env == o.env;
    }
    bool operator!=(const PrefixSettings& o) const { return !(*this == o); }
};

namespace LaunchSettings {

QStringList localePresets();

bool isValidEnvName(const QString& name);

// Переменные, которыми управляет сам Cask. Свои переменные их не заменяют.
const QStringList& reservedEnvNames();
bool isReservedEnvName(const QString& name);

// LANG и LC_ALL: именно их читает Wine при выборе языка.
void applyLocale(QProcessEnvironment& env, const QString& locale);

QString gamemodeBinary();    // путь к gamemoderun или пусто
bool localeInstalled(const QString& locale);  // есть ли такая локаль в `locale -a`

}  // namespace LaunchSettings
