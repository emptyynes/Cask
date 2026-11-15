#pragma once

#include <QList>
#include <QString>
#include <optional>

// Сборка Wine. Лежит в data/runners/<имя>/bin/wine, либо системный wine из PATH.
struct Runner {
    QString name;    // "system" или имя папки в data/runners
    QString binDir;  // папка с wine/wineserver
    QString wine;    // путь к исполняемому wine
    bool system = false;
};

namespace RunnerManager {

QList<Runner> available();
std::optional<Runner> find(const QString& name);

}  // namespace RunnerManager
