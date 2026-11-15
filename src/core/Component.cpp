#include "Component.h"

#include "Config.h"

#include <QCollator>
#include <QDir>
#include <algorithm>

namespace Components {

const QList<Type>& types()
{
    static const QList<Type> t = {
        {QStringLiteral("dxvk"), QStringLiteral("DXVK"),
         {QStringLiteral("x64")}, {QStringLiteral("x32"), QStringLiteral("x86")}},
        {QStringLiteral("d7vk"), QStringLiteral("D7VK"),
         {QStringLiteral("x64")}, {QStringLiteral("x32"), QStringLiteral("x86")}},
        {QStringLiteral("vkd3d-proton"), QStringLiteral("VKD3D-Proton"),
         {QStringLiteral("x64")}, {QStringLiteral("x86"), QStringLiteral("x32")}},
    };
    return t;
}

const Type* find(const QString& id)
{
    for (const auto& t : types())
        if (t.id == id)
            return &t;
    return nullptr;
}

QStringList versions(const QString& typeId)
{
    QDir d(Config::instance().componentsDir(typeId));
    QStringList v = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QCollator c;
    c.setNumericMode(true);
    c.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(v.begin(), v.end(), [&](const QString& a, const QString& b) { return c.compare(a, b) > 0; });
    return v;
}

}  // namespace Components
