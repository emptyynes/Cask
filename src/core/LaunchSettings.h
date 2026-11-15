#pragma once

#include <QMap>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

// Настройки запуска, хранятся у каждого префикса отдельно (cask.json -> "settings").
struct PrefixSettings {
    QString sync = QStringLiteral("fsync");  // none | esync | fsync | ntsync
    QString locale;                          // пусто = язык системы, иначе например ru_RU.UTF-8
    bool gamemode = false;                   // запускать программы через gamemoderun
    QMap<QString, QString> env;              // свои переменные окружения

    bool operator==(const PrefixSettings& o) const
    {
        return sync == o.sync && locale == o.locale && gamemode == o.gamemode && env == o.env;
    }
    bool operator!=(const PrefixSettings& o) const { return !(*this == o); }
};

namespace LaunchSettings {

const QStringList& syncModes();
QString normalizeSync(const QString& mode);  // неизвестное значение -> fsync (значение по умолчанию)

QStringList localePresets();

bool isValidEnvName(const QString& name);
// Эти переменные задаёт сам Cask, переопределять их нельзя.
bool isReservedEnvName(const QString& name);

// Переменные синхронизации. none = явно выключить всё.
void applySync(QProcessEnvironment& env, const QString& mode);
// LANG и LC_ALL: именно их читает Wine при выборе языка.
void applyLocale(QProcessEnvironment& env, const QString& locale);

bool ntsyncAvailable();      // есть ли доступный /dev/ntsync
QString gamemodeBinary();    // путь к gamemoderun или пусто
bool localeInstalled(const QString& locale);  // есть ли такая локаль в `locale -a`

}  // namespace LaunchSettings
