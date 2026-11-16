#include "Winetricks.h"

#include "Config.h"
#include "PrefixManager.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace Winetricks {

QString localDir() { return Config::instance().dataDir() + "/winetricks"; }
QString cacheDir() { return localDir() + "/cache"; }

QString scriptPath()
{
    const QString local = localDir() + "/winetricks";
    if (QFileInfo(local).isFile())
        return local;
    return QStandardPaths::findExecutable(QStringLiteral("winetricks"));
}

bool command(QString* program, QStringList* baseArgs)
{
    const QString path = scriptPath();
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).isExecutable()) {
        *program = path;
        baseArgs->clear();
    } else {
        *program = QStringLiteral("sh");
        *baseArgs = {path};
    }
    return true;
}

QProcessEnvironment makeEnv(const Prefix& p, const Runner& r)
{
    QProcessEnvironment e = PrefixManager::makeEnv(p, r, false);
    e.insert(QStringLiteral("WINE"), r.wine);
    e.insert(QStringLiteral("WINESERVER"), PrefixManager::wineserverPath(r));
    QDir().mkpath(cacheDir());
    e.insert(QStringLiteral("W_CACHE"), cacheDir());
    return e;
}

bool isValidVerb(const QString& verb)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.+\\-]*$"));
    return re.match(verb).hasMatch();
}

bool isManagedByCask(const QString& verb)
{
    return verb.startsWith(QLatin1String("dxvk")) || verb.startsWith(QLatin1String("vkd3d")) ||
           verb.startsWith(QLatin1String("d9vk"));
}

QList<Verb> parseList(const QByteArray& output)
{
    static const QRegularExpression header(QStringLiteral("^=+\\s*(\\S+)\\s*=+$"));
    static const QRegularExpression verbLine(QStringLiteral("^(\\S+)\\s+(.*)$"));

    // If the output has category headers, everything before the first one (winetricks banner,
    // warnings) is not treated as the list.
    const bool hasHeaders = output.contains("=====");

    QList<Verb> out;
    QString category;
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray& raw : lines) {
        const QString line = QString::fromLocal8Bit(raw).trimmed();
        if (line.isEmpty())
            continue;
        const QRegularExpressionMatch h = header.match(line);
        if (h.hasMatch()) {
            category = h.captured(1);
            continue;
        }
        if (hasHeaders && category.isEmpty())
            continue;
        const QRegularExpressionMatch m = verbLine.match(line);
        const QString name = m.hasMatch() ? m.captured(1) : line;
        if (!isValidVerb(name))
            continue;
        out.append({category, name, m.hasMatch() ? m.captured(2).trimmed() : QString()});
    }
    return out;
}

QSet<QString> parseInstalled(const QByteArray& output, const QSet<QString>& known)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    QSet<QString> out;
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray& raw : lines) {
        const QString line = QString::fromLocal8Bit(raw).trimmed();
        const QString name = line.section(whitespace, 0, 0);
        if (known.contains(name))
            out.insert(name);
    }
    return out;
}

}  // namespace Winetricks
