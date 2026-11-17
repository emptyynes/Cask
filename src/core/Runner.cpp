#include "Runner.h"

#include "Config.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace RunnerManager {

QList<Runner> available()
{
    QList<Runner> out;

    QDir d(Config::instance().runnersDir());
    const QStringList names = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    for (const QString& name : names) {
        // A plain Wine build (<name>/bin) or a Proton build (<name>/files/bin, e.g. Proton-GE).
        QString bin;
        QString wine;
        for (const char* sub : {"/bin", "/files/bin"}) {
            const QString dir = d.filePath(name) + QLatin1String(sub);
            for (const char* exe : {"/wine", "/wine64"}) {
                if (QFileInfo(dir + QLatin1String(exe)).isExecutable()) {
                    bin = dir;
                    wine = dir + QLatin1String(exe);
                    break;
                }
            }
            if (!wine.isEmpty())
                break;
        }
        if (wine.isEmpty())
            continue;
        out.append(Runner{name, bin, wine, false});
    }

    const QString sys = QStandardPaths::findExecutable(QStringLiteral("wine"));
    if (!sys.isEmpty())
        out.append(Runner{QStringLiteral("system"), QFileInfo(sys).absolutePath(), sys, true});

    return out;
}

std::optional<Runner> find(const QString& name)
{
    for (const Runner& r : available())
        if (r.name == name)
            return r;
    return std::nullopt;
}

}  // namespace RunnerManager
