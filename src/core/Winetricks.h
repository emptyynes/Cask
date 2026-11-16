#pragma once

#include "Prefix.h"
#include "Runner.h"

#include <QList>
#include <QProcessEnvironment>
#include <QSet>
#include <QString>
#include <QStringList>

// Winetricks runs as an external script: Cask does not download it itself (everything is manual).
// Uses <data>/winetricks/winetricks, otherwise the system `winetricks` from PATH.
namespace Winetricks {

struct Verb {
    QString category;     // apps, dlls, fonts, settings...; empty if the output has no categories
    QString name;
    QString description;
};

QString localDir();    // <data>/winetricks: the script goes here
QString cacheDir();    // <data>/winetricks/cache: winetricks downloads files here (W_CACHE)
QString scriptPath();  // the script found, or empty

// How to run the script. If the file is not executable, it is run via sh.
bool command(QString* program, QStringList* baseArgs);

// Environment: same as Cask internal tasks plus WINE, WINESERVER and W_CACHE.
QProcessEnvironment makeEnv(const Prefix& p, const Runner& r);

// Parses `winetricks list-all` output: headers like "===== dlls =====" and lines "verb  description".
QList<Verb> parseList(const QByteArray& output);
// Parses `winetricks list-installed` output: only names known from list-all are kept.
QSet<QString> parseInstalled(const QByteArray& output, const QSet<QString>& known);

bool isValidVerb(const QString& verb);
// dxvk, vkd3d-proton, d9vk: managed by Cask (the "Components" section), hidden from the list.
bool isManagedByCask(const QString& verb);

}  // namespace Winetricks
