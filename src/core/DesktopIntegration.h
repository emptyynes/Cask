#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// Desktop integration: the cask.desktop file and lines in mimeinfo.cache
// (both in ~/.local/share/applications, more precisely $XDG_DATA_HOME/applications).
//
// install() is idempotent: on every call it checks the binary path in .desktop and the cache lines
// against the expected ones and fixes only what differs (the Cask folder may have been moved).
// remove() deletes cask.desktop and removes only cask.desktop entries from mimeinfo.cache;
// mimeinfo.cache itself and other lines stay.
namespace DesktopIntegration {

QString applicationsDir();             // $XDG_DATA_HOME/applications
QString desktopFileName();             // "cask.desktop"
QStringList mimeTypes();               // types Cask takes over (exe, msi, bat, lnk)

QByteArray desktopContent(const QString& exePath);

// Pure function over mimeinfo.cache text: adds cask.desktop (at the front of each type list) or removes it.
QByteArray updateMimeCache(const QByteArray& cache, bool add);

bool isInstalled(const QString& appsDir);
bool install(const QString& appsDir, const QString& exePath, QString* err = nullptr);
bool remove(const QString& appsDir, QString* err = nullptr);
// enabled ? install : remove
bool sync(bool enabled, const QString& appsDir, const QString& exePath, QString* err = nullptr);

}  // namespace DesktopIntegration
