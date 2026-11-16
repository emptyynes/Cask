#pragma once

#include <QSettings>
#include <QString>
#include <memory>

// Портативная конфигурация.
//
// Корень Cask = папка с бинарником (или $CASK_ROOT):
//   Cask/
//     cask            бинарник
//     cask.ini        настройки (лежат рядом, не в ~/.config)
//     lib/            зависимости (Qt и т.п.)
//     data/           runners/, dxvk/, d7vk/, vkd3d-proton/
//     prefixes/       префиксы
//
// Пути data и prefixes можно переопределить в настройках. Если путь внутри
// корня, он хранится относительным, поэтому папку Cask можно переносить целиком.
class Config {
public:
    static Config& instance();

    QString rootDir() const { return m_root; }
    QString libDir() const;
    QString langDir() const;  // необязательные внешние переводы: lang/<код>.json
    QString dataDir() const;
    QString prefixesDir() const;
    QString runnersDir() const;
    QString componentsDir(const QString& typeId) const;

    void setDataDir(const QString& absPath);
    void setPrefixesDir(const QString& absPath);

    // "system" (по умолчанию, язык системы) или код языка ("en", "ru").
    QString language() const;
    void setLanguage(const QString& code);

    // Интеграция с рабочим столом (.desktop + mimeinfo.cache), по умолчанию выключена.
    bool desktopIntegration() const;
    void setDesktopIntegration(bool on);

    // Последний префикс, выбранный при запуске файла (предлагается первым).
    QString lastPrefix() const;
    void setLastPrefix(const QString& name);

    void ensureLayout() const;

private:
    Config();
    QString resolve(const QString& key, const QString& def) const;
    QString store(const QString& absPath) const;

    QString m_root;
    std::unique_ptr<QSettings> m_settings;
};
