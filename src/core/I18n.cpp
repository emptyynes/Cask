#include "I18n.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSet>
#include <QTranslator>

namespace I18n {

namespace {

class JsonTranslator : public QTranslator {
public:
    bool loadFile(const QString& path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return false;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject())
            return false;
        m_map.clear();
        const QJsonObject o = doc.object();
        for (auto it = o.begin(); it != o.end(); ++it) {
            if (it.key().startsWith(QLatin1Char('_')))
                continue;
            const QString v = it.value().toString();
            if (!v.isEmpty())
                m_map.insert(it.key(), v);
        }
        return true;
    }
    void clear() { m_map.clear(); }

    QString translate(const char* /*context*/, const char* sourceText, const char* /*disambiguation*/ = nullptr,
                      int /*n*/ = -1) const override
    {
        if (!sourceText)
            return {};
        const auto it = m_map.constFind(QString::fromUtf8(sourceText));
        return it == m_map.constEnd() ? QString() : it.value();
    }

    bool isEmpty() const override { return m_map.isEmpty(); }

private:
    QHash<QString, QString> m_map;
};

JsonTranslator& translator()
{
    // Deliberately never destroyed: it must outlive QCoreApplication during static destruction.
    static JsonTranslator* t = new JsonTranslator;
    return *t;
}

bool g_installed = false;
QString g_current = QStringLiteral("en");

QString findFile(const QString& code)
{
    const QString embedded = ":/lang/" + code + ".json";
    return QFileInfo::exists(embedded) ? embedded : QString();
}

QString readName(const QString& path, const QString& fallback)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return fallback;
    const QString n = QJsonDocument::fromJson(f.readAll()).object().value("_name").toString();
    return n.isEmpty() ? fallback : n;
}

}  // namespace

QList<Language> available()
{
    // English is the source language of the code and always comes first.
    QList<Language> out{{QStringLiteral("en"), QStringLiteral("English")}};
    const QDir d(QStringLiteral(":/lang"));
    const QStringList files = d.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString& f : files) {
        const QString code = QFileInfo(f).completeBaseName();
        if (code.isEmpty() || code == QLatin1String("en"))
            continue;
        out.append({code, readName(d.filePath(f), code)});
    }
    return out;
}

QString systemLanguage()
{
    QSet<QString> codes;
    for (const Language& l : available())
        codes.insert(l.code);
    const QStringList ui = QLocale::system().uiLanguages();
    for (const QString& u : ui) {
        const QString code = u.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0).toLower();
        if (codes.contains(code))
            return code;
    }
    return QStringLiteral("en");
}

QString resolve(const QString& setting)
{
    if (setting.isEmpty() || setting == QLatin1String("system"))
        return systemLanguage();
    for (const Language& l : available())
        if (l.code == setting)
            return setting;
    return QStringLiteral("en");
}

QString currentCode() { return g_current; }

bool apply(const QString& setting)
{
    const QString code = resolve(setting);
    JsonTranslator& t = translator();
    if (g_installed) {
        QCoreApplication::removeTranslator(&t);
        g_installed = false;
    }
    t.clear();
    g_current = QStringLiteral("en");
    if (code == QLatin1String("en"))
        return true;

    const QString path = findFile(code);
    if (path.isEmpty() || !t.loadFile(path))
        return false;
    QCoreApplication::installTranslator(&t);
    g_installed = true;
    g_current = code;
    return true;
}

}  // namespace I18n
