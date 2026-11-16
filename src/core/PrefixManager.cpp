#include "PrefixManager.h"

#include "Component.h"
#include "Config.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <algorithm>
#include <climits>
#include <csignal>
#include <sys/types.h>

namespace {

const char* kBak = ".cask-bak";

void setErr(QString* err, const QString& msg)
{
    if (err)
        *err = msg;
}

QStringList overrideNames(const QStringList& files)
{
    QSet<QString> s;
    for (const QString& f : files)
        s.insert(QFileInfo(f).completeBaseName().toLower());
    QStringList l = s.values();
    l.sort();
    return l;
}

// Рекурсивное удаление, которое НЕ заходит в симлинки
// (в префиксе dosdevices/z: указывает на "/").
bool removeTree(const QString& path)
{
    const QFileInfo fi(path);
    if (fi.isSymLink() || !fi.isDir())
        return QFile::remove(path);
    bool ok = true;
    const QDir d(path);
    const auto entries = d.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo& e : entries)
        ok = removeTree(e.absoluteFilePath()) && ok;
    return QDir().rmdir(path) && ok;
}

}  // namespace

PrefixManager::PrefixManager(QObject* parent) : QObject(parent) {}

QList<Prefix> PrefixManager::list() const
{
    QList<Prefix> out;
    const QDir d(Config::instance().prefixesDir());
    const QStringList names = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    for (const QString& n : names)
        if (auto p = Prefix::load(d.filePath(n)))
            out.append(*p);

    // Порядок пользователя; префиксы, которых в нём нет, идут после, по алфавиту.
    const QStringList ord = order();
    auto pos = [&ord](const Prefix& p) {
        const int i = ord.indexOf(p.name);
        return i < 0 ? INT_MAX : i;
    };
    std::stable_sort(out.begin(), out.end(), [&](const Prefix& a, const Prefix& b) { return pos(a) < pos(b); });
    return out;
}

QStringList PrefixManager::order() const
{
    QFile f(Config::instance().prefixesDir() + "/.cask-order.json");
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QStringList out;
    for (const QJsonValue& v : QJsonDocument::fromJson(f.readAll()).array())
        if (v.isString())
            out << v.toString();
    return out;
}

bool PrefixManager::setOrder(const QStringList& names)
{
    QFile f(Config::instance().prefixesDir() + "/.cask-order.json");
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(QJsonDocument(QJsonArray::fromStringList(names)).toJson(QJsonDocument::Indented));
    return true;
}

int PrefixManager::unlinkUserDirs(const QString& prefixPath)
{
    const QDir users(prefixPath + "/drive_c/users");
    if (!users.exists())
        return -1;

    const QString root = QDir::cleanPath(prefixPath);
    const QString canon = QFileInfo(prefixPath).canonicalFilePath();
    auto inside = [&](const QString& target) {
        const QString t = QDir::cleanPath(target);
        return t.startsWith(root + "/") || (!canon.isEmpty() && t.startsWith(canon + "/"));
    };

    int replaced = 0;
    const auto profiles = users.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& profile : profiles) {
        if (profile.isSymLink())
            continue;
        const QDir pd(profile.absoluteFilePath());
        const auto entries = pd.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
        for (const QFileInfo& e : entries) {
            // Ссылки внутри префикса (например "My Documents" -> "Documents") не трогаем.
            if (!e.isSymLink() || inside(e.symLinkTarget()))
                continue;
            if (QFile::remove(e.absoluteFilePath())) {  // удаляет саму ссылку, не её цель
                QDir().mkpath(e.absoluteFilePath());
                ++replaced;
            }
        }
    }
    return replaced;
}

QString PrefixManager::wineserverPath(const Runner& r)
{
    if (!r.system)
        return r.binDir + "/wineserver";
    const QString found = QStandardPaths::findExecutable(QStringLiteral("wineserver"));
    return found.isEmpty() ? QStringLiteral("wineserver") : found;
}

QList<qint64> PrefixManager::processes(const QString& prefixPath)
{
    // Любой процесс, в окружении которого WINEPREFIX указывает на этот префикс:
    // программы, запущенные из Cask, wineserver, winetricks и то, что запущено из терминала.
    QList<QByteArray> wanted{"WINEPREFIX=" + QDir::cleanPath(prefixPath).toLocal8Bit()};
    const QString canon = QFileInfo(prefixPath).canonicalFilePath();
    if (!canon.isEmpty() && canon != QDir::cleanPath(prefixPath))
        wanted << "WINEPREFIX=" + canon.toLocal8Bit();

    QList<qint64> out;
    const qint64 self = QCoreApplication::applicationPid();
    const QStringList entries = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& e : entries) {
        bool ok = false;
        const qint64 pid = e.toLongLong(&ok);
        if (!ok || pid == self)
            continue;
        QFile f(QStringLiteral("/proc/%1/environ").arg(pid));
        if (!f.open(QIODevice::ReadOnly))
            continue;  // чужой процесс или уже завершился
        const QList<QByteArray> vars = f.readAll().split('\0');
        for (const QByteArray& v : vars) {
            if (wanted.contains(v)) {
                out << pid;
                break;
            }
        }
    }
    return out;
}

int PrefixManager::killProcesses(const QString& prefixPath)
{
    int n = 0;
    for (qint64 pid : processes(prefixPath))
        if (::kill(static_cast<pid_t>(pid), SIGKILL) == 0)
            ++n;
    return n;
}

int PrefixManager::stopProcesses(const Prefix& p)
{
    const int before = processes(p.path).size();
    if (before == 0)
        return 0;

    // Сначала по-хорошему: wineserver -k завершает все процессы префикса.
    if (const auto r = RunnerManager::find(p.runner)) {
        QProcess k;
        k.setProcessEnvironment(makeEnv(p, *r, false));
        k.start(wineserverPath(*r), {"-k"});
        k.waitForFinished(5000);
    }
    // wineserver -k только рассылает сигнал: даём до 3 секунд, затем добиваем оставшихся.
    QElapsedTimer t;
    t.start();
    while (!processes(p.path).isEmpty() && t.elapsed() < 3000)
        QThread::msleep(100);
    killProcesses(p.path);
    return before;
}

bool PrefixManager::stopAll(const QString& name, QString* err)
{
    const auto p = find(name);
    if (!p) {
        setErr(err, tr("Prefix not found."));
        return false;
    }
    const int n = stopProcesses(*p);
    emit message(n == 0 ? tr("No running processes in prefix '%1'").arg(name)
                        : tr("All processes of prefix '%1' stopped").arg(name),
                 true);
    return true;
}

std::optional<Prefix> PrefixManager::find(const QString& name) const
{
    if (name.isEmpty() || name.contains('/') || name.startsWith('.'))
        return std::nullopt;
    return Prefix::load(Config::instance().prefixesDir() + "/" + name);
}

QProcessEnvironment PrefixManager::makeEnv(const Prefix& p, const Runner& r, bool forLaunch)
{
    QProcessEnvironment e = QProcessEnvironment::systemEnvironment();
    e.insert("WINEPREFIX", p.path);
    e.insert("WINEARCH", p.arch);
    if (!r.system)
        e.insert("PATH", r.binDir + ":" + e.value("PATH"));
    if (!e.contains("WINEDEBUG"))
        e.insert("WINEDEBUG", "-all");

    // Кеш шейдеров живёт внутри папки префикса (как в Bottles):
    //   <префикс>/cache/{dxvk_shader,mesa_shader,vkd3d_shader}
    // DXVK_STATE_CACHE_PATH: state cache DXVK до 2.7 (убран в 2.7), D7VK как форк
    // может его использовать. DXVK_SHADER_CACHE_PATH: кеш шейдеров DXVK 3.0+.
    const QString cache = p.path + "/cache";
    const QString dxvk = cache + "/dxvk_shader";
    const QString mesa = cache + "/mesa_shader";
    const QString vkd3d = cache + "/vkd3d_shader";
    QDir().mkpath(dxvk);
    QDir().mkpath(mesa);
    QDir().mkpath(vkd3d);
    e.insert("DXVK_STATE_CACHE_PATH", dxvk);
    e.insert("DXVK_SHADER_CACHE_PATH", dxvk);
    e.insert("MESA_SHADER_CACHE_DIR", mesa);
    e.insert("VKD3D_SHADER_CACHE_PATH", vkd3d);

    // Язык, который видит Wine. Синхронизацию (ntsync и др.) Cask не трогает.
    LaunchSettings::applyLocale(e, p.settings.locale);

    if (!forLaunch)
        return e;

    // Свои переменные префикса. Переменные, которыми управляет Cask (WINEPREFIX, PATH, язык,
    // пути кеша...), они не заменяют. Остальные, в том числе WINEDEBUG (Cask лишь ставит
    // для него значение по умолчанию), применяются.
    for (auto it = p.settings.env.cbegin(); it != p.settings.env.cend(); ++it) {
        if (LaunchSettings::isValidEnvName(it.key()) && !LaunchSettings::isReservedEnvName(it.key()))
            e.insert(it.key(), it.value());
    }
    return e;
}

void PrefixManager::startJob(const Prefix&, const Runner& r, const QStringList& args,
                             const QProcessEnvironment& env, const QString& okMsg,
                             std::function<void(bool)> after, const QString& workDir)
{
    auto* proc = new QProcess(this);
    proc->setProcessEnvironment(env);
    proc->setProgram(r.wine);
    proc->setArguments(args);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    if (!workDir.isEmpty())
        proc->setWorkingDirectory(workDir);

    auto finish = [this, proc, after](bool ok, const QString& msg) {
        emit message(msg, ok);
        if (after)
            after(ok);
        proc->deleteLater();
        if (--m_jobs == 0)
            emit busyChanged(false);
    };

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc, okMsg, finish](int code, QProcess::ExitStatus st) {
                const bool ok = st == QProcess::NormalExit && code == 0;
                QString msg = okMsg;
                if (!ok) {
                    const QString out = QString::fromLocal8Bit(proc->readAll()).trimmed().right(500);
                    msg = tr("Wine error (code %1). %2").arg(code).arg(out);
                }
                finish(ok, msg);
            });
    connect(proc, &QProcess::errorOccurred, this, [proc, finish, this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finish(false, tr("Failed to start %1").arg(proc->program()));
    });

    if (++m_jobs == 1)
        emit busyChanged(true);
    proc->start();
}

bool PrefixManager::create(const QString& name, const QString& runner, const QString& arch, QString* err)
{
    static const QRegularExpression re(QStringLiteral("^[\\p{L}\\p{N}][\\p{L}\\p{N} _.\\-]{0,63}$"));
    if (!re.match(name).hasMatch()) {
        setErr(err, tr("Invalid name. Letters, digits, space, _ . - are allowed (up to 64 characters)."));
        return false;
    }
    if (arch != "win64" && arch != "win32") {
        setErr(err, tr("Unknown architecture: %1").arg(arch));
        return false;
    }
    const auto r = RunnerManager::find(runner);
    if (!r) {
        setErr(err, tr("Wine '%1' was not found.").arg(runner));
        return false;
    }

    const QString path = Config::instance().prefixesDir() + "/" + name;
    if (QFileInfo::exists(path)) {
        setErr(err, tr("A prefix with this name already exists."));
        return false;
    }
    if (!QDir().mkpath(path)) {
        setErr(err, tr("Could not create folder %1").arg(path));
        return false;
    }

    Prefix p;
    p.name = name;
    p.path = path;
    p.runner = runner;
    p.arch = arch;
    p.created = QDateTime::currentDateTime();
    if (!p.save()) {
        setErr(err, tr("Could not write cask.json"));
        return false;
    }
    emit changed();

    QProcessEnvironment env = makeEnv(p, *r, false);
    env.insert("WINEDLLOVERRIDES", "mscoree,mshtml=d");  // без диалогов Mono/Gecko
    startJob(p, *r, {"wineboot", "--init"}, env, tr("Prefix '%1' created").arg(name),
             [this, path](bool ok) {
                 if (ok)
                     unlinkUserDirs(path);  // Desktop, Documents... не ссылаются на домашнюю папку
                 emit changed();
             });
    return true;
}

bool PrefixManager::remove(const QString& name, QString* err)
{
    const auto p = find(name);
    if (!p) {
        setErr(err, tr("Prefix not found."));
        return false;
    }
    if (isBusy()) {
        setErr(err, tr("Wait for the current operation to finish."));
        return false;
    }
    const QString root = QFileInfo(Config::instance().prefixesDir()).canonicalFilePath();
    const QString canon = QFileInfo(p->path).canonicalFilePath();
    if (root.isEmpty() || !canon.startsWith(root + "/")) {
        setErr(err, tr("The prefix path is outside the prefixes folder; deletion cancelled."));
        return false;
    }

    // Останавливаем все процессы префикса, чтобы ничего не осталось в удаляемой папке.
    stopProcesses(*p);

    if (!removeTree(p->path)) {
        setErr(err, tr("Not everything could be removed from %1").arg(p->path));
        emit changed();
        return false;
    }
    QStringList ord = order();
    if (ord.removeAll(name) > 0)
        setOrder(ord);
    emit changed();
    emit message(tr("Prefix '%1' removed").arg(name), true);
    return true;
}

bool PrefixManager::setRunner(const QString& name, const QString& runner, QString* err)
{
    auto p = find(name);
    if (!p) {
        setErr(err, tr("Prefix not found."));
        return false;
    }
    const auto r = RunnerManager::find(runner);
    if (!r) {
        setErr(err, tr("Wine '%1' was not found.").arg(runner));
        return false;
    }
    p->runner = runner;
    if (!p->save()) {
        setErr(err, tr("Could not write cask.json"));
        return false;
    }
    emit changed();
    startJob(*p, *r, {"wineboot", "-u"}, makeEnv(*p, *r, false),
             tr("Wine of prefix '%1' changed to %2").arg(name, runner), [this, path = p->path](bool ok) {
                 if (ok)
                     unlinkUserDirs(path);  // wineboot -u мог создать ссылки заново
                 emit changed();
             });
    return true;
}

// Откатывает файлы компонента. Возвращает имена overrides, которые больше
// никому не нужны (их нужно удалить из реестра).
QStringList PrefixManager::restoreFiles(Prefix& p, const QString& type)
{
    auto it = p.components.find(type);
    if (it == p.components.end())
        return {};

    QStringList names = overrideNames(it->files);
    const QString win = p.windowsDir();
    for (const QString& rel : std::as_const(it->files)) {
        const QString dst = win + "/" + rel;
        QFile::remove(dst);
        if (QFile::exists(dst + kBak))
            QFile::rename(dst + kBak, dst);
    }
    p.components.erase(it);

    QSet<QString> stillUsed;
    for (const AppliedComponent& c : std::as_const(p.components))
        for (const QString& n : overrideNames(c.files))
            stillUsed.insert(n);

    QStringList result;
    for (const QString& n : names)
        if (!stillUsed.contains(n))
            result << n;
    return result;
}

void PrefixManager::importOverrides(const Prefix& p, const Runner& r, const QStringList& setNative,
                                    const QStringList& removeNames, const QString& okMsg)
{
    if (setNative.isEmpty() && removeNames.isEmpty()) {
        emit message(okMsg, true);
        return;
    }

    QByteArray data = "Windows Registry Editor Version 5.00\r\n\r\n"
                      "[HKEY_CURRENT_USER\\Software\\Wine\\DllOverrides]\r\n";
    for (const QString& n : setNative)
        data += "\"" + n.toUtf8() + "\"=\"native,builtin\"\r\n";
    for (const QString& n : removeNames)
        data += "\"" + n.toUtf8() + "\"=-\r\n";

    // Файл передаём относительным именем с workDir = префикс: так надёжнее,
    // чем unix-путь в regedit.
    const QString regName = QStringLiteral("cask-overrides.reg");
    const QString regPath = p.path + "/" + regName;
    QFile f(regPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit message(tr("Could not write %1").arg(regPath), false);
        return;
    }
    f.write(data);
    f.close();

    startJob(p, r, {"regedit", "/S", regName}, makeEnv(p, r, false), okMsg,
             [regPath](bool) { QFile::remove(regPath); }, p.path);
}

bool PrefixManager::applyComponent(const QString& name, const QString& type, const QString& version, QString* err)
{
    auto po = find(name);
    if (!po) {
        setErr(err, tr("Prefix not found."));
        return false;
    }
    Prefix p = *po;
    const auto r = RunnerManager::find(p.runner);
    if (!r) {
        setErr(err, tr("The Wine of this prefix ('%1') was not found.").arg(p.runner));
        return false;
    }
    const Components::Type* t = Components::find(type);
    if (!t) {
        setErr(err, tr("Unknown component: %1").arg(type));
        return false;
    }
    const QString srcRoot = Config::instance().componentsDir(type) + "/" + version;
    if (!QDir(srcRoot).exists()) {
        setErr(err, tr("Folder not found: %1").arg(srcRoot));
        return false;
    }
    const QString win = p.windowsDir();
    if (!QDir(win + "/system32").exists()) {
        setErr(err, tr("The prefix is not initialized yet (no drive_c/windows/system32)."));
        return false;
    }

    // План копирования: (исходный файл, путь относительно windows/)
    struct Item { QString src; QString rel; };
    struct Map { QStringList srcDirs; QString dst; };
    QList<Map> maps;
    if (p.arch == "win64")
        maps = {{t->dirs64, "system32"}, {t->dirs32, "syswow64"}};
    else
        maps = {{t->dirs32, "system32"}};

    QList<Item> plan;
    for (const Map& m : std::as_const(maps)) {
        for (const QString& d : m.srcDirs) {
            const QDir sd(srcRoot + "/" + d);
            if (!sd.exists())
                continue;
            for (const QString& dll : sd.entryList({"*.dll"}, QDir::Files))
                plan.append({sd.filePath(dll), m.dst + "/" + dll});
            break;  // берём первую существующую подпапку
        }
    }
    if (plan.isEmpty()) {
        setErr(err, tr("No DLLs found in %1 (subfolders x64 and x32/x86 are expected).").arg(srcRoot));
        return false;
    }

    // Старую версию этого же компонента откатываем.
    const QStringList staleOverrides = restoreFiles(p, type);

    AppliedComponent a;
    a.version = version;
    for (const Item& it : std::as_const(plan)) {
        const QString dst = win + "/" + it.rel;
        if (QFile::exists(dst) && !QFile::exists(dst + kBak))
            QFile::rename(dst, dst + kBak);  // сохраняем оригинал один раз
        QFile::remove(dst);
        if (!QFile::copy(it.src, dst)) {
            p.components.insert(type, a);
            restoreFiles(p, type);
            p.save();
            setErr(err, tr("Could not copy %1").arg(it.src));
            emit changed();
            return false;
        }
        QFile::setPermissions(dst, QFile::permissions(dst) | QFile::WriteOwner);
        a.files << it.rel;
        // файл теперь принадлежит этому компоненту (последний применённый побеждает)
        for (auto c = p.components.begin(); c != p.components.end(); ++c)
            c->files.removeAll(it.rel);
    }
    p.components.insert(type, a);
    if (!p.save()) {
        setErr(err, tr("Could not write cask.json"));
        return false;
    }
    emit changed();

    const QStringList setNative = overrideNames(a.files);
    QStringList toRemove;
    for (const QString& n : staleOverrides)
        if (!setNative.contains(n))
            toRemove << n;
    importOverrides(p, *r, setNative, toRemove,
                    tr("%1 %2 applied to '%3'").arg(t->title, version, name));
    return true;
}

bool PrefixManager::removeComponent(const QString& name, const QString& type, QString* err)
{
    auto po = find(name);
    if (!po) {
        setErr(err, tr("Prefix not found."));
        return false;
    }
    Prefix p = *po;
    if (!p.components.contains(type)) {
        setErr(err, tr("Component is not applied."));
        return false;
    }
    const auto r = RunnerManager::find(p.runner);
    if (!r) {
        setErr(err, tr("The Wine of this prefix ('%1') was not found.").arg(p.runner));
        return false;
    }
    const QStringList drop = restoreFiles(p, type);
    if (!p.save()) {
        setErr(err, tr("Could not write cask.json"));
        return false;
    }
    emit changed();
    const Components::Type* t = Components::find(type);
    importOverrides(p, *r, {}, drop, tr("%1 removed from '%2'").arg(t ? t->title : type, name));
    return true;
}

bool PrefixManager::launch(const QString& name, const QStringList& wineArgs, const QString& workDir, QString* err,
                           bool game)
{
    const auto p = find(name);
    if (!p) {
        setErr(err, tr("Prefix not found."));
        return false;
    }
    const auto r = RunnerManager::find(p->runner);
    if (!r) {
        setErr(err, tr("The Wine of this prefix ('%1') was not found.").arg(p->runner));
        return false;
    }

    // Обёртки: gamemoderun mangohud wine ...
    QStringList chain;
    if (game && p->settings.gamemode) {
        const QString gm = LaunchSettings::gamemodeBinary();
        if (!gm.isEmpty())
            chain << gm;
        else
            emit message(tr("GameMode is enabled for this prefix, but gamemoderun was not found. "
                            "Launching without it."),
                         true);
    }
    bool portableHud = false;
    if (game && p->settings.mangohud) {
        if (LaunchSettings::hasPortableMangohud()) {
            portableHud = true;  // окружение добавляется ниже, обёртка не нужна
        } else {
            const QString mh = LaunchSettings::mangohudBinary();
            if (!mh.isEmpty())
                chain << mh;
            else
                emit message(tr("MangoHud is enabled for this prefix, but mangohud was not found. "
                                "Launching without it."),
                             true);
        }
    }
    chain << r->wine;
    const QString program = chain.takeFirst();
    const QStringList args = chain + wineArgs;

    QProcess proc;
    proc.setProgram(program);
    proc.setArguments(args);
    QProcessEnvironment env = makeEnv(*p, *r, true);
    if (portableHud && !LaunchSettings::applyPortableMangohud(env))
        emit message(tr("Could not set up the bundled MangoHud in %1. Launching without it.")
                         .arg(LaunchSettings::mangohudDir()),
                     true);
    if (game && p->settings.mangohud)
        LaunchSettings::applyMangohudConfig(env);
    proc.setProcessEnvironment(env);
    if (!workDir.isEmpty())
        proc.setWorkingDirectory(workDir);
    if (!proc.startDetached()) {
        setErr(err, tr("Failed to start %1").arg(program));
        return false;
    }
    return true;
}

QStringList PrefixManager::programArgs(const QString& file)
{
    return QFileInfo(file).suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0
               ? QStringList{file}
               : QStringList{QStringLiteral("start"), QStringLiteral("/unix"), file};
}

bool PrefixManager::setSettings(const QString& name, const PrefixSettings& in, QString* err)
{
    auto p = find(name);
    if (!p) {
        setErr(err, tr("Prefix not found."));
        return false;
    }

    PrefixSettings s = in;
    s.locale = s.locale.trimmed();
    for (auto it = s.env.cbegin(); it != s.env.cend(); ++it) {
        if (!LaunchSettings::isValidEnvName(it.key())) {
            setErr(err, tr("Invalid variable name: %1").arg(it.key()));
            return false;
        }
        if (LaunchSettings::isReservedEnvName(it.key())) {
            setErr(err, tr("The variable %1 is set by Cask and cannot be changed.").arg(it.key()));
            return false;
        }
    }

    if (s == p->settings)
        return true;
    p->settings = s;
    if (!p->save()) {
        setErr(err, tr("Could not write cask.json"));
        return false;
    }
    emit message(tr("Settings of prefix '%1' saved").arg(name), true);
    return true;
}
