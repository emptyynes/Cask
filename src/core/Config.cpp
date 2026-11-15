#include "Config.h"

#include "Component.h"

#include <QCoreApplication>
#include <QDir>

Config& Config::instance()
{
    static Config c;
    return c;
}

Config::Config()
{
    const QByteArray env = qgetenv("CASK_ROOT");
    m_root = env.isEmpty() ? QCoreApplication::applicationDirPath()
                           : QDir(QString::fromLocal8Bit(env)).absolutePath();
    m_root = QDir::cleanPath(m_root);
    m_settings = std::make_unique<QSettings>(m_root + "/cask.ini", QSettings::IniFormat);
}

QString Config::resolve(const QString& key, const QString& def) const
{
    QString v = m_settings->value(key).toString();
    if (v.isEmpty())
        v = def;
    return QDir::cleanPath(QDir::isAbsolutePath(v) ? v : m_root + "/" + v);
}

QString Config::store(const QString& absPath) const
{
    const QString clean = QDir::cleanPath(absPath);
    if (clean == m_root)
        return QStringLiteral(".");
    if (clean.startsWith(m_root + "/"))
        return clean.mid(m_root.size() + 1);
    return clean;
}

QString Config::libDir() const { return m_root + "/lib"; }
QString Config::dataDir() const { return resolve("paths/data", "data"); }
QString Config::prefixesDir() const { return resolve("paths/prefixes", "prefixes"); }
QString Config::runnersDir() const { return dataDir() + "/runners"; }
QString Config::componentsDir(const QString& typeId) const { return dataDir() + "/" + typeId; }

void Config::setDataDir(const QString& absPath)
{
    m_settings->setValue("paths/data", store(absPath));
    m_settings->sync();
}

void Config::setPrefixesDir(const QString& absPath)
{
    m_settings->setValue("paths/prefixes", store(absPath));
    m_settings->sync();
}

void Config::ensureLayout() const
{
    QDir().mkpath(libDir());
    QDir().mkpath(prefixesDir());
    QDir().mkpath(runnersDir());
    for (const auto& t : Components::types())
        QDir().mkpath(componentsDir(t.id));
}
