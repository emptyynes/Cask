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
    return p;
}
