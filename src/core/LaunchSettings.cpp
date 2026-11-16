#include "LaunchSettings.h"

#include "Config.h"

#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

namespace LaunchSettings {

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

QString mangohudBinary()
{
    return QStandardPaths::findExecutable(QStringLiteral("mangohud"));
}

QString mangohudDir()
{
    return Config::instance().dataDir() + QStringLiteral("/mangohud");
}

bool hasPortableMangohud()
{
    const QString lib = mangohudDir() + QStringLiteral("/lib64");
    return QFile::exists(lib + QStringLiteral("/libMangoHud_shim.so")) &&
           QFile::exists(lib + QStringLiteral("/libMangoHud.so"));
}

static void prependPath(QProcessEnvironment& env, const QString& var, const QString& dir, const QString& sep)
{
    const QString old = env.value(var);
    if (old.split(sep, Qt::SkipEmptyParts).contains(dir))
        return;
    env.insert(var, old.isEmpty() ? dir : dir + sep + old);
}

bool applyPortableMangohud(QProcessEnvironment& env)
{
    if (!hasPortableMangohud())
        return false;

    const QDir lib(mangohudDir() + QStringLiteral("/lib64"));
    const QString shim = lib.absoluteFilePath(QStringLiteral("libMangoHud_shim.so"));
    const QString layerDir = QDir(mangohudDir()).absoluteFilePath(QStringLiteral("layer"));

    // Манифест слоя из релиза указывает на /usr/lib/mangohud, поэтому пишем свой, с путём к нашей копии.
    const QByteArray json = QByteArrayLiteral(
                                "{\n"
                                "  \"file_format_version\": \"1.0.0\",\n"
                                "  \"layer\": {\n"
                                "    \"name\": \"VK_LAYER_MANGOHUD_overlay_x86_64\",\n"
                                "    \"type\": \"GLOBAL\",\n"
                                "    \"api_version\": \"1.3.0\",\n"
                                "    \"library_path\": \"") +
                            lib.absoluteFilePath(QStringLiteral("libMangoHud.so")).toUtf8() +
                            QByteArrayLiteral(
                                "\",\n"
                                "    \"implementation_version\": \"1\",\n"
                                "    \"description\": \"Vulkan Hud Overlay\",\n"
                                "    \"functions\": {\"vkNegotiateLoaderLayerInterfaceVersion\": "
                                "\"vkNegotiateLoaderLayerInterfaceVersion\"},\n"
                                "    \"enable_environment\": {\"MANGOHUD\": \"1\"},\n"
                                "    \"disable_environment\": {\"DISABLE_MANGOHUD\": \"1\"}\n"
                                "  }\n"
                                "}\n");
    QDir().mkpath(layerDir);
    const QString jsonPath = layerDir + QStringLiteral("/MangoHud.x86_64.json");
    QFile existing(jsonPath);
    const bool same = existing.open(QIODevice::ReadOnly) && existing.readAll() == json;
    existing.close();
    if (!same) {
        QFile f(jsonPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(json) != json.size())
            return false;
    }

    env.insert(QStringLiteral("MANGOHUD"), QStringLiteral("1"));
    // shim ищет libMangoHud_opengl.so рядом с собой; уже заданный LD_PRELOAD сохраняется.
    const QString pre = env.value(QStringLiteral("LD_PRELOAD"));
    if (!pre.split(':', Qt::SkipEmptyParts).contains(shim))
        env.insert(QStringLiteral("LD_PRELOAD"), pre.isEmpty() ? shim : pre + QLatin1Char(':') + shim);
    prependPath(env, QStringLiteral("VK_ADD_IMPLICIT_LAYER_PATH"), layerDir, QStringLiteral(":"));
    prependPath(env, QStringLiteral("VK_ADD_LAYER_PATH"), layerDir, QStringLiteral(":"));
    return true;
}

}  // namespace LaunchSettings
