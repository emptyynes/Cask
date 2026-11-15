#include "PrefixManager.h"

#include "Component.h"
#include "Config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>

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
    return out;
}

std::optional<Prefix> PrefixManager::find(const QString& name) const
{
    if (name.isEmpty() || name.contains('/') || name.startsWith('.'))
        return std::nullopt;
    return Prefix::load(Config::instance().prefixesDir() + "/" + name);
}

QProcessEnvironment PrefixManager::makeEnv(const Prefix& p, const Runner& r)
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
    // DXVK_STATE_CACHE_PATH читают и DXVK, и D7VK (форк DXVK).
    const QString cache = p.path + "/cache";
    const QString dxvk = cache + "/dxvk_shader";
    const QString mesa = cache + "/mesa_shader";
    const QString vkd3d = cache + "/vkd3d_shader";
    QDir().mkpath(dxvk);
    QDir().mkpath(mesa);
    QDir().mkpath(vkd3d);
    e.insert("DXVK_STATE_CACHE_PATH", dxvk);
    e.insert("MESA_SHADER_CACHE_DIR", mesa);
    e.insert("VKD3D_SHADER_CACHE_PATH", vkd3d);
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
                    msg = tr("Ошибка wine (код %1). %2").arg(code).arg(out);
                }
                finish(ok, msg);
            });
    connect(proc, &QProcess::errorOccurred, this, [proc, finish, this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finish(false, tr("Не удалось запустить %1").arg(proc->program()));
    });

    if (++m_jobs == 1)
        emit busyChanged(true);
    proc->start();
}

bool PrefixManager::create(const QString& name, const QString& runner, const QString& arch, QString* err)
{
    static const QRegularExpression re(QStringLiteral("^[\\p{L}\\p{N}][\\p{L}\\p{N} _.\\-]{0,63}$"));
    if (!re.match(name).hasMatch()) {
        setErr(err, tr("Недопустимое имя. Буквы, цифры, пробел, _ . - (до 64 символов)."));
        return false;
    }
    if (arch != "win64" && arch != "win32") {
        setErr(err, tr("Неизвестная архитектура: %1").arg(arch));
        return false;
    }
    const auto r = RunnerManager::find(runner);
    if (!r) {
        setErr(err, tr("Wine «%1» не найден.").arg(runner));
        return false;
    }

    const QString path = Config::instance().prefixesDir() + "/" + name;
    if (QFileInfo::exists(path)) {
        setErr(err, tr("Префикс с таким именем уже существует."));
        return false;
    }
    if (!QDir().mkpath(path)) {
        setErr(err, tr("Не удалось создать папку %1").arg(path));
        return false;
    }

    Prefix p;
    p.name = name;
    p.path = path;
    p.runner = runner;
    p.arch = arch;
    p.created = QDateTime::currentDateTime();
    if (!p.save()) {
        setErr(err, tr("Не удалось записать cask.json"));
        return false;
    }
    emit changed();

    QProcessEnvironment env = makeEnv(p, *r);
    env.insert("WINEDLLOVERRIDES", "mscoree,mshtml=d");  // без диалогов Mono/Gecko
    startJob(p, *r, {"wineboot", "--init"}, env, tr("Префикс «%1» создан").arg(name),
             [this](bool) { emit changed(); });
    return true;
}

bool PrefixManager::remove(const QString& name, QString* err)
{
    const auto p = find(name);
    if (!p) {
        setErr(err, tr("Префикс не найден."));
        return false;
    }
    if (isBusy()) {
        setErr(err, tr("Дождитесь завершения текущей операции."));
        return false;
    }
    const QString root = QFileInfo(Config::instance().prefixesDir()).canonicalFilePath();
    const QString canon = QFileInfo(p->path).canonicalFilePath();
    if (root.isEmpty() || !canon.startsWith(root + "/")) {
        setErr(err, tr("Путь префикса вне папки префиксов, удаление отменено."));
        return false;
    }

    // Останавливаем wineserver префикса, чтобы не осталось процессов.
    if (const auto r = RunnerManager::find(p->runner)) {
        QProcess k;
        k.setProcessEnvironment(makeEnv(*p, *r));
        k.start(r->system ? QStringLiteral("wineserver") : r->binDir + "/wineserver", {"-k"});
        k.waitForFinished(5000);
    }

    if (!removeTree(p->path)) {
        setErr(err, tr("Не всё удалось удалить из %1").arg(p->path));
        emit changed();
        return false;
    }
    emit changed();
    emit message(tr("Префикс «%1» удалён").arg(name), true);
    return true;
}

bool PrefixManager::setRunner(const QString& name, const QString& runner, QString* err)
{
    auto p = find(name);
    if (!p) {
        setErr(err, tr("Префикс не найден."));
        return false;
    }
    const auto r = RunnerManager::find(runner);
    if (!r) {
        setErr(err, tr("Wine «%1» не найден.").arg(runner));
        return false;
    }
    p->runner = runner;
    if (!p->save()) {
        setErr(err, tr("Не удалось записать cask.json"));
        return false;
    }
    emit changed();
    startJob(*p, *r, {"wineboot", "-u"}, makeEnv(*p, *r),
             tr("Wine префикса «%1» изменён на %2").arg(name, runner), [this](bool) { emit changed(); });
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
        emit message(tr("Не удалось записать %1").arg(regPath), false);
        return;
    }
    f.write(data);
    f.close();

    startJob(p, r, {"regedit", "/S", regName}, makeEnv(p, r), okMsg,
             [regPath](bool) { QFile::remove(regPath); }, p.path);
}

bool PrefixManager::applyComponent(const QString& name, const QString& type, const QString& version, QString* err)
{
    auto po = find(name);
    if (!po) {
        setErr(err, tr("Префикс не найден."));
        return false;
    }
    Prefix p = *po;
    const auto r = RunnerManager::find(p.runner);
    if (!r) {
        setErr(err, tr("Wine «%1» этого префикса не найден.").arg(p.runner));
        return false;
    }
    const Components::Type* t = Components::find(type);
    if (!t) {
        setErr(err, tr("Неизвестный компонент: %1").arg(type));
        return false;
    }
    const QString srcRoot = Config::instance().componentsDir(type) + "/" + version;
    if (!QDir(srcRoot).exists()) {
        setErr(err, tr("Папка не найдена: %1").arg(srcRoot));
        return false;
    }
    const QString win = p.windowsDir();
    if (!QDir(win + "/system32").exists()) {
        setErr(err, tr("Префикс ещё не инициализирован (нет drive_c/windows/system32)."));
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
        setErr(err, tr("В %1 не найдено DLL (ожидаются подпапки x64 и x32/x86).").arg(srcRoot));
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
            setErr(err, tr("Не удалось скопировать %1").arg(it.src));
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
        setErr(err, tr("Не удалось записать cask.json"));
        return false;
    }
    emit changed();

    const QStringList setNative = overrideNames(a.files);
    QStringList toRemove;
    for (const QString& n : staleOverrides)
        if (!setNative.contains(n))
            toRemove << n;
    importOverrides(p, *r, setNative, toRemove,
                    tr("%1 %2 применён к «%3»").arg(t->title, version, name));
    return true;
}

bool PrefixManager::removeComponent(const QString& name, const QString& type, QString* err)
{
    auto po = find(name);
    if (!po) {
        setErr(err, tr("Префикс не найден."));
        return false;
    }
    Prefix p = *po;
    if (!p.components.contains(type)) {
        setErr(err, tr("Компонент не применён."));
        return false;
    }
    const auto r = RunnerManager::find(p.runner);
    if (!r) {
        setErr(err, tr("Wine «%1» этого префикса не найден.").arg(p.runner));
        return false;
    }
    const QStringList drop = restoreFiles(p, type);
    if (!p.save()) {
        setErr(err, tr("Не удалось записать cask.json"));
        return false;
    }
    emit changed();
    const Components::Type* t = Components::find(type);
    importOverrides(p, *r, {}, drop, tr("%1 удалён из «%2»").arg(t ? t->title : type, name));
    return true;
}

bool PrefixManager::launch(const QString& name, const QStringList& wineArgs, const QString& workDir, QString* err)
{
    const auto p = find(name);
    if (!p) {
        setErr(err, tr("Префикс не найден."));
        return false;
    }
    const auto r = RunnerManager::find(p->runner);
    if (!r) {
        setErr(err, tr("Wine «%1» этого префикса не найден.").arg(p->runner));
        return false;
    }
    QProcess proc;
    proc.setProgram(r->wine);
    proc.setArguments(wineArgs);
    proc.setProcessEnvironment(makeEnv(*p, *r));
    if (!workDir.isEmpty())
        proc.setWorkingDirectory(workDir);
    if (!proc.startDetached()) {
        setErr(err, tr("Не удалось запустить %1").arg(r->wine));
        return false;
    }
    return true;
}
