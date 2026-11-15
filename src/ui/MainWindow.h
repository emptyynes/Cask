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
    void stopAll();
    void openWinetricks();
    void saveSettings();
    void editEnv();

private:
    // Строка компонента: список «Никакой / версии». Выбор версии применяет её,
    // выбор «Никакой» снимает компонент.
    struct CompRow {
        QString type;
        QComboBox* versions = nullptr;
        QLabel* hint = nullptr;
    };

    void buildUi();
    void retranslate();  // пересобирает интерфейс после смены языка
    void runTool(const QStringList& wineArgs);
    void componentChosen(const QString& type);
    void saveOrder();
    void selectByName(const QString& name);
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
    QComboBox* m_localeBox = nullptr;
    QCheckBox* m_gamemode = nullptr;
    QCheckBox* m_mangohud = nullptr;
    QPushButton* m_envBtn = nullptr;
    QMap<QString, QString> m_env;
    bool m_loading = false;  // true, пока элементы заполняются из префикса (не сохранять)

    QProgressBar* m_busy = nullptr;
};
