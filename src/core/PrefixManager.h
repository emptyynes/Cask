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

    QList<Prefix> list() const;
    std::optional<Prefix> find(const QString& name) const;

    // Создаёт папку префикса и в фоне запускает wineboot --init.
    bool create(const QString& name, const QString& runner, const QString& arch, QString* err);
    bool remove(const QString& name, QString* err);

    // Меняет Wine у префикса и обновляет его (wineboot -u) в фоне.
    bool setRunner(const QString& name, const QString& runner, QString* err);

    // Копирует DLL из data/<type>/<version> в префикс и прописывает overrides.
    bool applyComponent(const QString& name, const QString& type, const QString& version, QString* err);
    bool removeComponent(const QString& name, const QString& type, QString* err);

    // Запуск wine в префиксе (отсоединённо): программа, winecfg, regedit...
    bool launch(const QString& name, const QStringList& wineArgs, const QString& workDir, QString* err);

    bool isBusy() const { return m_jobs > 0; }

signals:
    void changed();
    void busyChanged(bool busy);
    void message(const QString& text, bool ok);

private:
    static QProcessEnvironment makeEnv(const Prefix& p, const Runner& r);
    void startJob(const Prefix& p, const Runner& r, const QStringList& args, const QProcessEnvironment& env,
                  const QString& okMsg, std::function<void(bool)> after = {}, const QString& workDir = {});
    void importOverrides(const Prefix& p, const Runner& r, const QStringList& setNative,
                         const QStringList& removeNames, const QString& okMsg);
    QStringList restoreFiles(Prefix& p, const QString& type);

    int m_jobs = 0;
};
