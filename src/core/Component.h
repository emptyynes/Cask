#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Components (DXVK, D7VK, VKD3D-Proton) are placed manually in
// data/<type>/<version>/{x64,x32|x86}/*.dll
namespace Components {

struct Type {
    QString id;          // folder name in data/
    QString title;       // display name
    QStringList dirs64;  // subfolders with 64-bit DLLs
    QStringList dirs32;  // subfolders with 32-bit DLLs
};

const QList<Type>& types();
const Type* find(const QString& id);
QStringList versions(const QString& typeId);  // newest versions first

}  // namespace Components
