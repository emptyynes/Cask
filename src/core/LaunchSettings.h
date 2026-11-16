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
    bool mangohud = false;                   // запускать программы через mangohud
    QMap<QString, QString> env;              // свои переменные окружения

    bool operator==(const PrefixSettings& o) const
    {
        return locale == o.locale && gamemode == o.gamemode && mangohud == o.mangohud && env == o.env;
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
QString mangohudBinary();    // путь к mangohud или пусто

// Портативный MangoHud (только 64 бит): data/mangohud/lib64/libMangoHud{,_shim,_opengl}.so
QString mangohudDir();       // <data>/mangohud
bool hasPortableMangohud();  // есть ли data/mangohud/lib64/libMangoHud_shim.so и libMangoHud.so
// Добавляет в env: MANGOHUD=1, LD_PRELOAD с shim (для OpenGL) и путь к слою Vulkan
// (слой с абсолютным путём к библиотеке создаётся в <data>/mangohud/layer при каждом вызове).
bool applyPortableMangohud(QProcessEnvironment& env);

// Если есть <data>/mangohud/MangoHud.conf и MANGOHUD_CONFIGFILE не задан, указывает на него.
// Работает и с системным mangohud. Возвращает true, если переменная выставлена.
QString mangohudConfigPath();
bool applyMangohudConfig(QProcessEnvironment& env);

}  // namespace LaunchSettings
