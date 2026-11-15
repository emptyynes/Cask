#include "core/Config.h"
#include "core/I18n.h"
#include "core/LaunchSettings.h"
#include "core/Prefix.h"
#include "core/PrefixManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class CoreTest : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    Prefix makePrefix(const QString& name) const
    {
        Prefix p;
        p.name = name;
        p.path = m_dir.path() + "/prefixes/" + name;
        QDir().mkpath(p.path);
        return p;
    }
    static Runner systemRunner() { return Runner{QStringLiteral("system"), QStringLiteral("/usr/bin"),
                                                 QStringLiteral("/usr/bin/wine"), true}; }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        qputenv("CASK_ROOT", m_dir.path().toLocal8Bit());
        Config::instance().ensureLayout();
    }

    void prefixSettingsRoundTrip()
    {
        Prefix p = makePrefix("roundtrip");
        p.settings.sync = "ntsync";
        p.settings.locale = "ru_RU.UTF-8";
        p.settings.gamemode = true;
        p.settings.env = {{"DXVK_HUD", "fps"}, {"MANGOHUD", "1"}};
        QVERIFY(p.save());

        const auto loaded = Prefix::load(p.path);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->settings, p.settings);
    }

    void oldPrefixWithoutSettingsLoadsDefaults()
    {
        Prefix p = makePrefix("legacy");
        QFile f(p.path + "/cask.json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"runner":"system","arch":"win64","created":"2026-01-01T00:00:00","components":{}})");
        f.close();

        const auto loaded = Prefix::load(p.path);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->settings.sync, QString("fsync"));
        QVERIFY(loaded->settings.locale.isEmpty());
        QVERIFY(!loaded->settings.gamemode);
        QVERIFY(loaded->settings.env.isEmpty());
    }

    void syncModes_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<QString>("esync");
        QTest::addColumn<QString>("fsync");
        QTest::addColumn<QString>("ntsync");
        QTest::newRow("none") << "none" << "0" << "0" << "0";
        QTest::newRow("esync") << "esync" << "1" << "0" << "0";
        QTest::newRow("fsync") << "fsync" << "0" << "1" << "0";
        QTest::newRow("ntsync") << "ntsync" << "0" << "0" << "1";
        QTest::newRow("garbage falls back to fsync") << "bogus" << "0" << "1" << "0";
    }
    void syncModes()
    {
        QFETCH(QString, mode);
        QFETCH(QString, esync);
        QFETCH(QString, fsync);
        QFETCH(QString, ntsync);

        Prefix p = makePrefix("sync");
        p.settings.sync = mode;
        const QProcessEnvironment e = PrefixManager::makeEnv(p, systemRunner(), true);
        QCOMPARE(e.value("WINEESYNC"), esync);
        QCOMPARE(e.value("WINEFSYNC"), fsync);
        QCOMPARE(e.value("WINENTSYNC"), ntsync);
    }

    void localeIsPassedToWine()
    {
        Prefix p = makePrefix("locale");
        p.settings.locale = "ru_RU.UTF-8";
        for (bool launch : {false, true}) {
            const QProcessEnvironment e = PrefixManager::makeEnv(p, systemRunner(), launch);
            QCOMPARE(e.value("LANG"), QString("ru_RU.UTF-8"));
            QCOMPARE(e.value("LC_ALL"), QString("ru_RU.UTF-8"));
        }
    }

    void emptyLocaleKeepsSystemLanguage()
    {
        Prefix p = makePrefix("nolocale");
        const QProcessEnvironment sys = QProcessEnvironment::systemEnvironment();
        const QProcessEnvironment e = PrefixManager::makeEnv(p, systemRunner(), true);
        QCOMPARE(e.value("LANG"), sys.value("LANG"));
        QCOMPARE(e.value("LC_ALL"), sys.value("LC_ALL"));
    }

    void userEnvOverridesButNotReserved()
    {
        Prefix p = makePrefix("userenv");
        p.settings.env = {{"DXVK_HUD", "fps"},
                          {"WINEDEBUG", "+timestamp"},
                          {"WINEFSYNC", "0"},  // переопределяет настройку синхронизации
                          {"WINEPREFIX", "/evil"},
                          {"WINEARCH", "win32"},
                          {"BAD NAME", "x"}};
        p.settings.sync = "fsync";
        const QProcessEnvironment e = PrefixManager::makeEnv(p, systemRunner(), true);
        QCOMPARE(e.value("DXVK_HUD"), QString("fps"));
        QCOMPARE(e.value("WINEDEBUG"), QString("+timestamp"));
        QCOMPARE(e.value("WINEFSYNC"), QString("0"));
        QCOMPARE(e.value("WINEPREFIX"), p.path);
        QCOMPARE(e.value("WINEARCH"), p.arch);
        QVERIFY(!e.contains("BAD NAME"));
    }

    void internalJobsIgnoreUserEnv()
    {
        Prefix p = makePrefix("internal");
        p.settings.env = {{"LD_PRELOAD", "/nonexistent.so"}};
        const QProcessEnvironment e = PrefixManager::makeEnv(p, systemRunner(), false);
        QVERIFY(e.value("LD_PRELOAD") != QString("/nonexistent.so"));
    }

    void cacheLivesInsidePrefix()
    {
        Prefix p = makePrefix("cache");
        const QProcessEnvironment e = PrefixManager::makeEnv(p, systemRunner(), false);
        QCOMPARE(e.value("DXVK_SHADER_CACHE_PATH"), p.path + "/cache/dxvk_shader");
        QCOMPARE(e.value("DXVK_STATE_CACHE_PATH"), p.path + "/cache/dxvk_shader");
        QCOMPARE(e.value("MESA_SHADER_CACHE_DIR"), p.path + "/cache/mesa_shader");
        QCOMPARE(e.value("VKD3D_SHADER_CACHE_PATH"), p.path + "/cache/vkd3d_shader");
        QVERIFY(QDir(p.path + "/cache/mesa_shader").exists());
    }

    void envNameValidation()
    {
        QVERIFY(LaunchSettings::isValidEnvName("DXVK_HUD"));
        QVERIFY(LaunchSettings::isValidEnvName("_x1"));
        QVERIFY(!LaunchSettings::isValidEnvName(""));
        QVERIFY(!LaunchSettings::isValidEnvName("1ABC"));
        QVERIFY(!LaunchSettings::isValidEnvName("A-B"));
        QVERIFY(!LaunchSettings::isValidEnvName("A B"));
        QVERIFY(LaunchSettings::isReservedEnvName("WINEPREFIX"));
        QVERIFY(!LaunchSettings::isReservedEnvName("WINEDEBUG"));
    }

    void settingsAreValidatedAndSaved()
    {
        Prefix p = makePrefix("manager");
        QVERIFY(p.save());
        PrefixManager mgr;

        PrefixSettings s;
        s.sync = "esync";
        s.env = {{"FOO", "bar"}};
        QString err;
        QVERIFY2(mgr.setSettings("manager", s, &err), qPrintable(err));
        QCOMPARE(mgr.find("manager")->settings, s);

        PrefixSettings bad = s;
        bad.env = {{"WINEPREFIX", "/x"}};
        QVERIFY(!mgr.setSettings("manager", bad, &err));
        bad.env = {{"NOT VALID", "x"}};
        QVERIFY(!mgr.setSettings("manager", bad, &err));
        QCOMPARE(mgr.find("manager")->settings, s);  // неудачные попытки ничего не меняют

        QVERIFY(!mgr.setSettings("missing", s, &err));
    }

    void languagesAreAvailable()
    {
        const auto langs = I18n::available();
        QVERIFY(langs.size() >= 2);
        QCOMPARE(langs.first().code, QString("en"));
        bool hasRu = false;
        for (const auto& l : langs)
            hasRu = hasRu || l.code == "ru";
        QVERIFY(hasRu);
    }

    void russianTranslationWorksAndEnglishRestores()
    {
        const QString src = "Settings";
        QVERIFY(I18n::apply("ru"));
        QCOMPARE(I18n::currentCode(), QString("ru"));
        QCOMPARE(QCoreApplication::translate("X", "Settings"), QString::fromUtf8("Настройки"));
        QCOMPARE(QCoreApplication::translate("X", "Applied: %1").arg("DXVK"), QString::fromUtf8("Применён: DXVK"));

        QVERIFY(I18n::apply("en"));
        QCOMPARE(QCoreApplication::translate("X", "Settings"), src);
        QCOMPARE(I18n::currentCode(), QString("en"));
    }

    void unknownLanguageFallsBackToEnglish()
    {
        QCOMPARE(I18n::resolve("xx"), QString("en"));
        QVERIFY(I18n::apply("xx"));
        QCOMPARE(QCoreApplication::translate("X", "Settings"), QString("Settings"));
    }

    void externalLanguageFileOverridesAndExtends()
    {
        QFile f(Config::instance().langDir() + "/de.json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"_name":"Deutsch","Settings":"Einstellungen"})");
        f.close();

        bool hasDe = false;
        for (const auto& l : I18n::available())
            if (l.code == "de") {
                hasDe = true;
                QCOMPARE(l.name, QString("Deutsch"));
            }
        QVERIFY(hasDe);
        QVERIFY(I18n::apply("de"));
        QCOMPARE(QCoreApplication::translate("X", "Settings"), QString("Einstellungen"));
        // строки, которых нет в файле, остаются английскими
        QCOMPARE(QCoreApplication::translate("X", "Refresh"), QString("Refresh"));
        QVERIFY(I18n::apply("en"));
    }
};

QTEST_GUILESS_MAIN(CoreTest)
#include "test_core.moc"
