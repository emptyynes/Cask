#include "core/Config.h"
#include "core/I18n.h"
#include "core/LaunchSettings.h"
#include "core/Prefix.h"
#include "core/PrefixManager.h"
#include "core/Winetricks.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
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

    static void writeFile(const QString& path, const QByteArray& data)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        qputenv("CASK_ROOT", m_dir.path().toLocal8Bit());
        Config::instance().ensureLayout();
    }

    // ---- настройки префикса ----

    void prefixSettingsRoundTrip()
    {
        Prefix p = makePrefix("roundtrip");
        p.settings.locale = "ru_RU.UTF-8";
        p.settings.gamemode = true;
        p.settings.mangohud = true;
        p.settings.env = {{"DXVK_HUD", "fps"}, {"MANGOHUD", "1"}};
        QVERIFY(p.save());

        const auto loaded = Prefix::load(p.path);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->settings, p.settings);
    }

    void handEditedPrefixDropsProtectedAndInvalidVariables()
    {
        Prefix p = makePrefix("handedited");
        writeFile(p.path + "/cask.json",
                  R"({"runner":"system","arch":"win64","created":"2026-01-01T00:00:00","components":{},
                      "settings":{"env":{"LANG":"C","KEEP":"1","bad name":"x"}}})");

        const auto loaded = Prefix::load(p.path);
        QVERIFY(loaded.has_value());
        QVERIFY(loaded->settings.locale.isEmpty());
        QVERIFY(!loaded->settings.gamemode);
        QVERIFY(!loaded->settings.mangohud);
        QCOMPARE(loaded->settings.env, (QMap<QString, QString>{{"KEEP", "1"}}));
    }

    // ---- окружение ----

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

    void cask_doesNotTouchSyncVariables()
    {
        Prefix p = makePrefix("nosync");
        const QProcessEnvironment sys = QProcessEnvironment::systemEnvironment();
        const QProcessEnvironment e = PrefixManager::makeEnv(p, systemRunner(), true);
        for (const char* v : {"WINEESYNC", "WINEFSYNC", "WINENTSYNC", "WINE_DISABLE_FAST_SYNC"})
            QCOMPARE(e.value(v), sys.value(v));
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

    void userEnvNeverReplacesCaskVariables()
    {
        Prefix p = makePrefix("protected");
        p.settings.locale = "ru_RU.UTF-8";
        // Все защищённые имена пробуем подменить.
        for (const QString& name : LaunchSettings::reservedEnvNames())
            p.settings.env.insert(name, "/hacked");
        p.settings.env.insert("DXVK_HUD", "fps");

        const QProcessEnvironment e = PrefixManager::makeEnv(p, systemRunner(), true);
        QCOMPARE(e.value("WINEPREFIX"), p.path);
        QCOMPARE(e.value("WINEARCH"), p.arch);
        QCOMPARE(e.value("LANG"), QString("ru_RU.UTF-8"));
        QCOMPARE(e.value("LC_ALL"), QString("ru_RU.UTF-8"));
        QCOMPARE(e.value("DXVK_SHADER_CACHE_PATH"), p.path + "/cache/dxvk_shader");
        QCOMPARE(e.value("DXVK_STATE_CACHE_PATH"), p.path + "/cache/dxvk_shader");
        QCOMPARE(e.value("MESA_SHADER_CACHE_DIR"), p.path + "/cache/mesa_shader");
        QCOMPARE(e.value("VKD3D_SHADER_CACHE_PATH"), p.path + "/cache/vkd3d_shader");
        QVERIFY(e.value("PATH") != QString("/hacked"));
        // а обычные переменные применяются
        QCOMPARE(e.value("DXVK_HUD"), QString("fps"));
    }

    void userEnvAppliesOrdinaryVariables()
    {
        Prefix p = makePrefix("userenv");
        p.settings.env = {{"WINEDEBUG", "+timestamp"}, {"WINEDLLOVERRIDES", "winemenubuilder.exe=d"},
                          {"BAD NAME", "x"}};
        const QProcessEnvironment e = PrefixManager::makeEnv(p, systemRunner(), true);
        QCOMPARE(e.value("WINEDEBUG"), QString("+timestamp"));  // у Cask лишь значение по умолчанию
        QCOMPARE(e.value("WINEDLLOVERRIDES"), QString("winemenubuilder.exe=d"));
        QVERIFY(!e.contains("BAD NAME"));
    }

    void internalJobsIgnoreUserEnv()
    {
        Prefix p = makePrefix("internal");
        p.settings.env = {{"LD_PRELOAD", "/nonexistent.so"}};
        const QProcessEnvironment e = PrefixManager::makeEnv(p, systemRunner(), false);
        QVERIFY(e.value("LD_PRELOAD") != QString("/nonexistent.so"));
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
        QVERIFY(LaunchSettings::isReservedEnvName("LC_ALL"));
        QVERIFY(LaunchSettings::isReservedEnvName("MESA_SHADER_CACHE_DIR"));
        QVERIFY(!LaunchSettings::isReservedEnvName("DXVK_HUD"));
        QVERIFY(!LaunchSettings::isReservedEnvName("WINEDEBUG"));
    }

    void settingsAreValidatedAndSaved()
    {
        Prefix p = makePrefix("manager");
        QVERIFY(p.save());
        PrefixManager mgr;

        PrefixSettings s;
        s.gamemode = true;
        s.mangohud = true;
        s.env = {{"FOO", "bar"}};
        QString err;
        QVERIFY2(mgr.setSettings("manager", s, &err), qPrintable(err));
        QCOMPARE(mgr.find("manager")->settings, s);

        PrefixSettings bad = s;
        bad.env = {{"WINEPREFIX", "/x"}};
        QVERIFY(!mgr.setSettings("manager", bad, &err));
        bad.env = {{"LANG", "C"}};  // язык меняется отдельной настройкой
        QVERIFY(!mgr.setSettings("manager", bad, &err));
        bad.env = {{"NOT VALID", "x"}};
        QVERIFY(!mgr.setSettings("manager", bad, &err));
        QCOMPARE(mgr.find("manager")->settings, s);  // неудачные попытки ничего не меняют

        QVERIFY(!mgr.setSettings("missing", s, &err));
    }

    // ---- папки профиля Wine ----

    void userDirsAreUnlinkedFromHome()
    {
        Prefix p = makePrefix("isolate");
        const QString home = m_dir.path() + "/fake-home";
        writeFile(home + "/Desktop/keep.txt", "mine");
        writeFile(home + "/Documents/keep.txt", "mine");

        const QString user = p.path + "/drive_c/users/tester";
        QDir().mkpath(user + "/AppData");
        QVERIFY(QFile::link(home + "/Desktop", user + "/Desktop"));
        QVERIFY(QFile::link(home + "/Documents", user + "/Documents"));
        QVERIFY(QFile::link(home + "/Nowhere", user + "/Downloads"));         // битая ссылка наружу
        QVERIFY(QFile::link(user + "/Documents", user + "/My Documents"));    // ссылка внутри префикса

        QCOMPARE(PrefixManager::unlinkUserDirs(p.path), 3);

        for (const char* d : {"Desktop", "Documents", "Downloads"}) {
            const QFileInfo fi(user + "/" + d);
            QVERIFY2(fi.isDir() && !fi.isSymLink(), d);
            QVERIFY2(QDir(fi.absoluteFilePath()).isEmpty(), d);  // пустая обычная папка
        }
        // файлы пользователя по ссылкам не тронуты
        QVERIFY(QFile::exists(home + "/Desktop/keep.txt"));
        QVERIFY(QFile::exists(home + "/Documents/keep.txt"));
        // ссылка внутри префикса осталась
        QVERIFY(QFileInfo(user + "/My Documents").isSymLink());
        QVERIFY(QDir(user + "/AppData").exists());

        QCOMPARE(PrefixManager::unlinkUserDirs(p.path), 0);  // повторный вызов ничего не меняет
    }

    void unlinkUserDirsWithoutProfile()
    {
        Prefix p = makePrefix("noprofile");
        QCOMPARE(PrefixManager::unlinkUserDirs(p.path), -1);
    }

    // ---- остановка процессов префикса ----

    void processesAreFoundAndKilledPerPrefix()
    {
        const QString pathA = makePrefix("kill_a").path;
        const QString pathB = makePrefix("kill_b").path;

        auto startSleep = [](QProcess& proc, const QString& prefix) {
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("WINEPREFIX", prefix);
            proc.setProcessEnvironment(env);
            proc.start("sleep", {"60"});
            return proc.waitForStarted(3000);
        };
        QProcess a, b;
        QVERIFY(startSleep(a, pathA));
        QVERIFY(startSleep(b, pathB));

        QTRY_VERIFY(PrefixManager::processes(pathA).contains(a.processId()));
        QVERIFY(!PrefixManager::processes(pathA).contains(b.processId()));  // чужой префикс не виден

        QVERIFY(PrefixManager::killProcesses(pathA) >= 1);
        QVERIFY(a.waitForFinished(3000));
        QCOMPARE(b.state(), QProcess::Running);                              // и не затронут
        QVERIFY(PrefixManager::processes(pathA).isEmpty());

        b.kill();
        b.waitForFinished(3000);
    }

    // ---- winetricks ----

    void winetricksListIsParsed()
    {
        const QByteArray out =
            "Using winetricks 20240105 - sha256sum: abc with wine-11.22 and WINEARCH=win64\n"
            "===== apps =====\n"
            "7zip                         7-Zip 9.20 (7-zip.org, 2010) [downloadable]\n"
            "\n"
            "===== dlls =====\n"
            "corefonts                    MS Arial, Courier, Times fonts [downloadable]\n"
            "dxvk                         Vulkan-based D3D implementation (2024) [downloadable]\n"
            "vcrun2019                    Visual C++ 2015-2019 libraries (Microsoft, 2019)\n";
        const auto verbs = Winetricks::parseList(out);
        QCOMPARE(verbs.size(), 4);  // строка-баннер до первого заголовка не попала в список
        QCOMPARE(verbs[0].category, QString("apps"));
        QCOMPARE(verbs[0].name, QString("7zip"));
        QCOMPARE(verbs[0].description, QString("7-Zip 9.20 (7-zip.org, 2010) [downloadable]"));
        QCOMPARE(verbs[1].category, QString("dlls"));
        QCOMPARE(verbs[3].name, QString("vcrun2019"));

        // вывод без заголовков тоже разбирается
        const auto flat = Winetricks::parseList("corefonts  MS fonts\nvcrun2019  VC++\n");
        QCOMPARE(flat.size(), 2);
        QVERIFY(flat[0].category.isEmpty());
    }

    void winetricksInstalledOnlyKnownVerbs()
    {
        const QSet<QString> known{"corefonts", "vcrun2019", "dxvk"};
        const auto installed = Winetricks::parseInstalled(
            "corefonts\nvcrun2019\nwarning: something odd\n\n", known);
        QCOMPARE(installed, (QSet<QString>{"corefonts", "vcrun2019"}));
    }

    void winetricksHidesWhatCaskManages()
    {
        QVERIFY(Winetricks::isManagedByCask("dxvk"));
        QVERIFY(Winetricks::isManagedByCask("dxvk_nvapi"));
        QVERIFY(Winetricks::isManagedByCask("vkd3d"));
        QVERIFY(Winetricks::isManagedByCask("d9vk"));
        QVERIFY(!Winetricks::isManagedByCask("corefonts"));
        QVERIFY(!Winetricks::isManagedByCask("d3dx9"));
        QVERIFY(!Winetricks::isManagedByCask("d3dcompiler_47"));
        QVERIFY(Winetricks::isValidVerb("vcrun2019"));
        QVERIFY(!Winetricks::isValidVerb("a b"));
        QVERIFY(!Winetricks::isValidVerb("-q"));
        QVERIFY(!Winetricks::isValidVerb("; rm"));
    }

    void winetricksEnvironment()
    {
        Prefix p = makePrefix("wt");
        const Runner r{QStringLiteral("custom"), QStringLiteral("/opt/w/bin"), QStringLiteral("/opt/w/bin/wine"),
                       false};
        const QProcessEnvironment e = Winetricks::makeEnv(p, r);
        QCOMPARE(e.value("WINEPREFIX"), p.path);
        QCOMPARE(e.value("WINE"), QString("/opt/w/bin/wine"));
        QCOMPARE(e.value("WINESERVER"), QString("/opt/w/bin/wineserver"));
        QCOMPARE(e.value("W_CACHE"), Winetricks::cacheDir());
        QVERIFY(Winetricks::cacheDir().startsWith(Config::instance().dataDir()));
        QVERIFY(e.value("PATH").startsWith("/opt/w/bin:"));
    }

    // ---- порядок префиксов ----

    void prefixOrderIsRememberedAndNewOnesGoLast()
    {
        for (const char* n : {"ord_a", "ord_b", "ord_c"})
            QVERIFY(makePrefix(n).save());
        PrefixManager mgr;

        auto positions = [&mgr] {
            QStringList names;
            for (const Prefix& p : mgr.list())
                if (p.name.startsWith("ord_"))
                    names << p.name;
            return names;
        };

        QCOMPARE(positions(), (QStringList{"ord_a", "ord_b", "ord_c"}));  // по алфавиту
        QVERIFY(mgr.setOrder({"ord_c", "ord_a", "ord_b"}));
        QCOMPARE(positions(), (QStringList{"ord_c", "ord_a", "ord_b"}));

        QVERIFY(makePrefix("ord_d").save());  // новый, его нет в порядке
        QCOMPARE(positions(), (QStringList{"ord_c", "ord_a", "ord_b", "ord_d"}));

        QCOMPARE(mgr.order(), (QStringList{"ord_c", "ord_a", "ord_b"}));
    }

    // ---- переводы ----

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
        QVERIFY(I18n::apply("ru"));
        QCOMPARE(I18n::currentCode(), QString("ru"));
        QCOMPARE(QCoreApplication::translate("X", "Settings"), QString::fromUtf8("Настройки"));
        QCOMPARE(QCoreApplication::translate("X", "None"), QString::fromUtf8("Никакой"));
        QCOMPARE(QCoreApplication::translate("X", "Explorer"), QString::fromUtf8("Проводник"));
        QCOMPARE(QCoreApplication::translate("X", "Control panel"), QString::fromUtf8("Панель управления"));

        QVERIFY(I18n::apply("en"));
        QCOMPARE(QCoreApplication::translate("X", "Settings"), QString("Settings"));
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
        writeFile(Config::instance().langDir() + "/de.json",
                  R"({"_name":"Deutsch","Settings":"Einstellungen"})");

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
