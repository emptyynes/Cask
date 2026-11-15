#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Компоненты (DXVK, D7VK, VKD3D-Proton) кладутся вручную в
// data/<тип>/<версия>/{x64,x32|x86}/*.dll
namespace Components {

struct Type {
    QString id;          // имя папки в data/
    QString title;       // отображаемое имя
    QStringList dirs64;  // подпапки с 64-битными DLL
    QStringList dirs32;  // подпапки с 32-битными DLL
};

const QList<Type>& types();
const Type* find(const QString& id);
QStringList versions(const QString& typeId);  // новые версии первыми

}  // namespace Components
