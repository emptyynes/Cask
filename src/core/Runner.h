#pragma once

#include <QList>
#include <QString>
#include <optional>

// A Wine build. Lives in data/runners/<name>/bin/wine (or <name>/files/bin/wine for a Proton build),
// or is the system wine from PATH.
struct Runner {
    QString name;    // "system" or the folder name in data/runners
    QString binDir;  // folder with wine/wineserver
    QString wine;    // path to the wine executable
    bool system = false;
};

namespace RunnerManager {

QList<Runner> available();
std::optional<Runner> find(const QString& name);

}  // namespace RunnerManager
