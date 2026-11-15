#pragma once

#include "core/PrefixManager.h"

#include <QList>
#include <QMainWindow>
#include <QMap>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSplitter;
class QToolBar;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void refresh();
    void onSelected();
    void newPrefix();
    void deletePrefix();
    void openSettings();
    void runExe();
    void openFolder();
    void applyRunner();
    void saveSettings();
    void editEnv();

private:
    struct CompRow {
        QString type;
        QComboBox* versions = nullptr;
        QLabel* state = nullptr;
        QPushButton* apply = nullptr;
        QPushButton* remove = nullptr;
    };

    void buildUi();
    void retranslate();  // пересобирает интерфейс после смены языка
    void runTool(const QStringList& wineArgs);
    void applyComponent(const QString& type);
    void removeComponent(const QString& type);
    void selectByName(const QString& name);
    void updateWarnings();
    void updateEnvButton();
    QString localeValue() const;
    void setLocaleValue(const QString& value);
    QString currentName() const;

    PrefixManager m_mgr;
    QToolBar* m_toolbar = nullptr;
    QSplitter* m_central = nullptr;
    QListWidget* m_list = nullptr;
    QWidget* m_details = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_path = nullptr;
    QLabel* m_arch = nullptr;
    QComboBox* m_runnerBox = nullptr;
    QList<CompRow> m_rows;

    // Настройки запуска
    QComboBox* m_syncBox = nullptr;
    QLabel* m_syncWarn = nullptr;
    QComboBox* m_localeBox = nullptr;
    QLabel* m_localeWarn = nullptr;
    QCheckBox* m_gamemode = nullptr;
    QPushButton* m_envBtn = nullptr;
    QMap<QString, QString> m_env;
    bool m_loading = false;  // true, пока элементы заполняются из префикса (не сохранять)

    QProgressBar* m_busy = nullptr;
};
