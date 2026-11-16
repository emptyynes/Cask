#pragma once

#include "LaunchSettings.h"

#include <QDateTime>
#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>

// An applied component: files are recorded relative to drive_c/windows
// (e.g. "system32/d3d11.dll") so they can be rolled back.
struct AppliedComponent {
    QString version;
    QStringList files;
};

// A prefix = a prefixes/<name>/ folder (also the WINEPREFIX) with cask.json metadata.
// The absolute path is not stored, so the prefixes folder can be moved.
struct Prefix {
    QString name;
    QString path;
    QString runner = QStringLiteral("system");
    QString arch = QStringLiteral("win64");
    QDateTime created;
    QMap<QString, AppliedComponent> components;
    PrefixSettings settings;

    QString windowsDir() const { return path + "/drive_c/windows"; }

    bool save() const;
    static std::optional<Prefix> load(const QString& dir);
};
