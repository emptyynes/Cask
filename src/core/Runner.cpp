#include "Runner.h"

#include "Config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace RunnerManager {

namespace {

// True for a 32-bit ELF executable (the "wine" loader of builds that keep 32-bit and 64-bit loaders apart).
bool is32BitElf(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = f.read(5);
    return head.size() == 5 && head.startsWith("\x7f""ELF") && head.at(4) == 1;
}

}  // namespace

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
        // On a system without 32-bit libraries the 32-bit "wine" loader cannot even start
        // ("/lib/ld-linux.so.2: could not open"): use the 64-bit "wine64" of the same build instead.
        if (wine.endsWith(QLatin1String("/wine")) && is32BitElf(wine) && !QFileInfo::exists(QStringLiteral("/lib/ld-linux.so.2"))
            && QFileInfo(bin + "/wine64").isExecutable())
            wine = bin + "/wine64";
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
