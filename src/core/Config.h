#pragma once

#include <QSettings>
#include <QString>
#include <memory>

// Portable configuration.
//
// Cask root = the folder with the binary (or $CASK_ROOT):
//   Cask/
//     cask            the binary
//     cask.ini        settings (stored alongside, not in ~/.config)
//     lib/            dependencies (Qt etc.)
//     data/           runners/, dxvk/, d7vk/, vkd3d-proton/
//     prefixes/       prefixes
//
// The data and prefixes paths can be overridden in settings. If a path is inside
// the root, it is stored as relative, so the whole Cask folder can be moved.
class Config {
public:
    static Config& instance();

    QString rootDir() const { return m_root; }
    QString libDir() const;
    QString dataDir() const;
    QString prefixesDir() const;
    QString runnersDir() const;
    QString componentsDir(const QString& typeId) const;

    void setDataDir(const QString& absPath);
    void setPrefixesDir(const QString& absPath);

    // "system" (default, the system language) or a language code ("en", "ru").
    QString language() const;
    void setLanguage(const QString& code);

    // Desktop integration (.desktop + mimeinfo.cache), off by default.
    bool desktopIntegration() const;
    void setDesktopIntegration(bool on);

    // The last prefix selected when launching a file (offered first).
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
