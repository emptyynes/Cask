#include "LaunchSettings.h"

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace LaunchSettings {

namespace {

QString normLocale(QString s)
{
    s = s.trimmed().toLower();
    s.replace(QStringLiteral("utf-8"), QStringLiteral("utf8"));
    return s;
}

}  // namespace

const QStringList& syncModes()
{
    static const QStringList m{QStringLiteral("none"), QStringLiteral("esync"), QStringLiteral("fsync"),
                               QStringLiteral("ntsync")};
    return m;
}

QString normalizeSync(const QString& mode)
{
    const QString m = mode.trimmed().toLower();
    return syncModes().contains(m) ? m : QStringLiteral("fsync");
}

QStringList localePresets()
{
    return {QStringLiteral("en_US.UTF-8"), QStringLiteral("ru_RU.UTF-8"), QStringLiteral("uk_UA.UTF-8"),
            QStringLiteral("de_DE.UTF-8"), QStringLiteral("fr_FR.UTF-8"), QStringLiteral("es_ES.UTF-8"),
            QStringLiteral("it_IT.UTF-8"), QStringLiteral("pt_BR.UTF-8"), QStringLiteral("pl_PL.UTF-8"),
            QStringLiteral("tr_TR.UTF-8"), QStringLiteral("zh_CN.UTF-8"), QStringLiteral("ja_JP.UTF-8"),
            QStringLiteral("ko_KR.UTF-8")};
}

bool isValidEnvName(const QString& name)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    return re.match(name).hasMatch();
}

bool isReservedEnvName(const QString& name)
{
    return name == QLatin1String("WINEPREFIX") || name == QLatin1String("WINEARCH");
}

void applySync(QProcessEnvironment& env, const QString& mode)
{
    const QString m = normalizeSync(mode);
    const auto flag = [&](const char* mine) { return m == QLatin1String(mine) ? QStringLiteral("1") : QStringLiteral("0"); };
    env.insert(QStringLiteral("WINEESYNC"), flag("esync"));
    env.insert(QStringLiteral("WINEFSYNC"), flag("fsync"));
    env.insert(QStringLiteral("WINENTSYNC"), flag("ntsync"));
    if (m == QLatin1String("ntsync"))
        env.insert(QStringLiteral("WINE_DISABLE_FAST_SYNC"), QStringLiteral("0"));
}

void applyLocale(QProcessEnvironment& env, const QString& locale)
{
    const QString l = locale.trimmed();
    if (l.isEmpty())
        return;
    env.insert(QStringLiteral("LANG"), l);
    env.insert(QStringLiteral("LC_ALL"), l);
}

bool ntsyncAvailable()
{
    const QFileInfo fi(QStringLiteral("/dev/ntsync"));
    return fi.exists() && fi.isReadable() && fi.isWritable();
}

QString gamemodeBinary()
{
    return QStandardPaths::findExecutable(QStringLiteral("gamemoderun"));
}

bool localeInstalled(const QString& locale)
{
    const QString want = normLocale(locale);
    if (want.isEmpty() || want == QLatin1String("c") || want == QLatin1String("posix"))
        return true;

    static const QStringList installed = [] {
        QStringList list;
        QProcess p;
        p.start(QStringLiteral("locale"), {QStringLiteral("-a")});
        if (!p.waitForFinished(3000)) {
            p.kill();
            return list;
        }
        const QList<QByteArray> lines = p.readAllStandardOutput().split('\n');
        for (const QByteArray& line : lines)
            if (!line.trimmed().isEmpty())
                list << normLocale(QString::fromLocal8Bit(line));
        return list;
    }();

    if (installed.isEmpty())
        return true;  // `locale` недоступен: не знаем, не пугаем пользователя
    return installed.contains(want);
}

}  // namespace LaunchSettings
