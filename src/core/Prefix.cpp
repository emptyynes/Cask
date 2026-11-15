#include "Prefix.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
const char* kMetaFile = "cask.json";
}

bool Prefix::save() const
{
    QJsonObject o;
    o["runner"] = runner;
    o["arch"] = arch;
    o["created"] = created.toString(Qt::ISODate);

    QJsonObject comps;
    for (auto it = components.cbegin(); it != components.cend(); ++it) {
        QJsonObject c;
        c["version"] = it->version;
        c["files"] = QJsonArray::fromStringList(it->files);
        comps[it.key()] = c;
    }
    o["components"] = comps;

    QJsonObject s;
    s["locale"] = settings.locale;
    s["gamemode"] = settings.gamemode;
    QJsonObject env;
    for (auto it = settings.env.cbegin(); it != settings.env.cend(); ++it)
        env[it.key()] = it.value();
    s["env"] = env;
    o["settings"] = s;

    QFile f(path + "/" + kMetaFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return true;
}

std::optional<Prefix> Prefix::load(const QString& dir)
{
    QFile f(dir + "/" + kMetaFile);
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return std::nullopt;
    const QJsonObject o = doc.object();

    Prefix p;
    p.path = QDir::cleanPath(dir);
    p.name = QFileInfo(p.path).fileName();  // имя = имя папки
    p.runner = o.value("runner").toString("system");
    p.arch = o.value("arch").toString("win64");
    p.created = QDateTime::fromString(o.value("created").toString(), Qt::ISODate);

    const QJsonObject comps = o.value("components").toObject();
    for (auto it = comps.begin(); it != comps.end(); ++it) {
        const QJsonObject c = it.value().toObject();
        AppliedComponent a;
        a.version = c.value("version").toString();
        for (const QJsonValue& v : c.value("files").toArray())
            a.files << v.toString();
        p.components.insert(it.key(), a);
    }

    const QJsonObject s = o.value("settings").toObject();
    p.settings.locale = s.value("locale").toString();
    p.settings.gamemode = s.value("gamemode").toBool(false);
    const QJsonObject env = s.value("env").toObject();
    for (auto it = env.begin(); it != env.end(); ++it) {
        // Если cask.json правили руками: переменные, которыми управляет Cask, отбрасываются.
        if (LaunchSettings::isValidEnvName(it.key()) && !LaunchSettings::isReservedEnvName(it.key()))
            p.settings.env.insert(it.key(), it.value().toString());
    }
    return p;
}
