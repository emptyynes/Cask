#pragma once

#include "Prefix.h"
#include "Runner.h"

#include <QList>
#include <QObject>
#include <QProcessEnvironment>
#include <functional>
#include <optional>

class PrefixManager : public QObject {
    Q_OBJECT
public:
    explicit PrefixManager(QObject* parent = nullptr);

    // Список в порядке, заданном пользователем (setOrder); новые префиксы в конце.
    QList<Prefix> list() const;
    std::optional<Prefix> find(const QString& name) const;

    // Порядок префиксов хранится рядом с ними: <папка префиксов>/.cask-order.json
    QStringList order() const;
    bool setOrder(const QStringList& names);

    // Заменяет симлинки Desktop, Documents, Downloads... в профиле Wine, ведущие за пределы
    // префикса (в домашнюю папку), обычными пустыми папками. Файлы по ссылкам не трогает.
    // Возвращает число замен, -1 если профиль ещё не создан.
    static int unlinkUserDirs(const QString& prefixPath);

    // Останавливает все процессы префикса: wineserver -k, затем SIGKILL тем, кто остался.
    bool stopAll(const QString& name, QString* err);
    // pid процессов, у которых WINEPREFIX указывает на этот префикс.
    static QList<qint64> processes(const QString& prefixPath);
    // SIGKILL всем таким процессам; возвращает число успешно сигналенных.
    static int killProcesses(const QString& prefixPath);
    // wineserver выбранной сборки Wine.
    static QString wineserverPath(const Runner& r);

    // Создаёт папку префикса и в фоне запускает wineboot --init.
    bool create(const QString& name, const QString& runner, const QString& arch, QString* err);
    bool remove(const QString& name, QString* err);

    // Меняет Wine у префикса и обновляет его (wineboot -u) в фоне.
    bool setRunner(const QString& name, const QString& runner, QString* err);

    // Копирует DLL из data/<type>/<version> в префикс и прописывает overrides.
    bool applyComponent(const QString& name, const QString& type, const QString& version, QString* err);
    bool removeComponent(const QString& name, const QString& type, QString* err);

    // Запуск wine в префиксе (отсоединённо): программа, winecfg, regedit...
    // game = true: пользовательская программа, к ней применяются GameMode и MangoHud (если включены у префикса).
    bool launch(const QString& name, const QStringList& wineArgs, const QString& workDir, QString* err,
                bool game = false);

    // Сохраняет настройки запуска префикса (переменные, синхронизация, язык, GameMode).
    bool setSettings(const QString& name, const PrefixSettings& settings, QString* err);

    // Окружение для wine. forLaunch = false: внутренние задачи (wineboot, regedit),
    // только база, кеш и язык. true: запуск пользователем, плюс свои переменные
    // (они не заменяют переменные, которыми управляет Cask).
    static QProcessEnvironment makeEnv(const Prefix& p, const Runner& r, bool forLaunch);

    bool isBusy() const { return m_jobs > 0; }

signals:
    void changed();
    void busyChanged(bool busy);
    void message(const QString& text, bool ok);

private:
    int stopProcesses(const Prefix& p);  // возвращает, сколько процессов работало до остановки
    void startJob(const Prefix& p, const Runner& r, const QStringList& args, const QProcessEnvironment& env,
                  const QString& okMsg, std::function<void(bool)> after = {}, const QString& workDir = {});
    void importOverrides(const Prefix& p, const Runner& r, const QStringList& setNative,
                         const QStringList& removeNames, const QString& okMsg);
    QStringList restoreFiles(Prefix& p, const QString& type);

    int m_jobs = 0;
};
