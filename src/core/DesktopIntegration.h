#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// Интеграция с рабочим столом: файл cask.desktop и строки в mimeinfo.cache
// (оба в ~/.local/share/applications, точнее в $XDG_DATA_HOME/applications).
//
// install() идемпотентна: при каждом вызове сверяет путь к бинарнику в .desktop и строки кеша
// с ожидаемыми и исправляет только то, что отличается (папку Cask могли переместить).
// remove() удаляет cask.desktop и убирает из mimeinfo.cache только записи cask.desktop;
// сам mimeinfo.cache и чужие строки остаются.
namespace DesktopIntegration {

QString applicationsDir();             // $XDG_DATA_HOME/applications
QString desktopFileName();             // "cask.desktop"
QStringList mimeTypes();               // типы, которые Cask берёт на себя (exe, msi, bat, lnk)

QByteArray desktopContent(const QString& exePath);

// Чистая функция над текстом mimeinfo.cache: добавляет (в начало списка каждого типа) или убирает cask.desktop.
QByteArray updateMimeCache(const QByteArray& cache, bool add);

bool isInstalled(const QString& appsDir);
bool install(const QString& appsDir, const QString& exePath, QString* err = nullptr);
bool remove(const QString& appsDir, QString* err = nullptr);
// enabled ? install : remove
bool sync(bool enabled, const QString& appsDir, const QString& exePath, QString* err = nullptr);

}  // namespace DesktopIntegration
