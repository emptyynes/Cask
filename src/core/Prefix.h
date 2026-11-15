#pragma once

#include "LaunchSettings.h"

#include <QDateTime>
#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>

// Применённый компонент: файлы записаны относительно drive_c/windows
// (например "system32/d3d11.dll"), чтобы их можно было откатить.
struct AppliedComponent {
    QString version;
    QStringList files;
};

// Префикс = папка prefixes/<имя>/ (она же WINEPREFIX) с метаданными cask.json.
// Абсолютный путь не хранится, поэтому папку префиксов можно переносить.
struct Prefix {
    QString name;
    QString path;
    QString runner = QStringLiteral("system");
    QString arch = QStringLiteral("win64");
    QDateTime created;
    QMap<QString, AppliedComponent> components;
    PrefixSettings settings;
    // Симлинки Desktop/Documents/... на домашнюю папку уже заменены обычными папками.
    bool userDirsIsolated = false;

    QString windowsDir() const { return path + "/drive_c/windows"; }

    bool save() const;
    static std::optional<Prefix> load(const QString& dir);
};
