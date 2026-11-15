#pragma once

#include "Prefix.h"
#include "Runner.h"

#include <QList>
#include <QProcessEnvironment>
#include <QSet>
#include <QString>
#include <QStringList>

// Winetricks запускается как внешний скрипт: Cask не скачивает его сам (всё вручную).
// Берётся <data>/winetricks/winetricks, иначе системный `winetricks` из PATH.
namespace Winetricks {

struct Verb {
    QString category;     // apps, dlls, fonts, settings...; пусто, если вывод без категорий
    QString name;
    QString description;
};

QString localDir();    // <data>/winetricks: сюда кладётся скрипт
QString cacheDir();    // <data>/winetricks/cache: сюда winetricks качает файлы (W_CACHE)
QString scriptPath();  // найденный скрипт или пусто

// Как запустить скрипт. Если у файла нет права на запуск, он выполняется через sh.
bool command(QString* program, QStringList* baseArgs);

// Окружение: как у внутренних задач Cask плюс WINE, WINESERVER и W_CACHE.
QProcessEnvironment makeEnv(const Prefix& p, const Runner& r);

// Разбор вывода `winetricks list-all`: заголовки вида "===== dlls =====" и строки "verb  описание".
QList<Verb> parseList(const QByteArray& output);
// Разбор вывода `winetricks list-installed`: берутся только имена, известные из list-all.
QSet<QString> parseInstalled(const QByteArray& output, const QSet<QString>& known);

bool isValidVerb(const QString& verb);
// dxvk, vkd3d-proton, d9vk: ими управляет Cask (раздел «Компоненты»), в списке они скрыты.
bool isManagedByCask(const QString& verb);

}  // namespace Winetricks
