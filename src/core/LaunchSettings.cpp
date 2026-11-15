#include "LaunchSettings.h"

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

const QStringList& reservedEnvNames()
{
    // Должен совпадать с тем, что выставляет PrefixManager::makeEnv.
    static const QStringList names{
        QStringLiteral("WINEPREFIX"),
        QStringLiteral("WINEARCH"),
        QStringLiteral("PATH"),
        QStringLiteral("LANG"),
        QStringLiteral("LC_ALL"),
        QStringLiteral("DXVK_STATE_CACHE_PATH"),
        QStringLiteral("DXVK_SHADER_CACHE_PATH"),
        QStringLiteral("MESA_SHADER_CACHE_DIR"),
        QStringLiteral("VKD3D_SHADER_CACHE_PATH"),
    };
    return names;
}

bool isReservedEnvName(const QString& name)
{
    return reservedEnvNames().contains(name);
}

void applyLocale(QProcessEnvironment& env, const QString& locale)
{
    const QString l = locale.trimmed();
    if (l.isEmpty())
        return;
    env.insert(QStringLiteral("LANG"), l);
    env.insert(QStringLiteral("LC_ALL"), l);
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
