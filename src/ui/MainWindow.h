#pragma once

#include "core/PrefixManager.h"

#include <QList>
#include <QMainWindow>

class QComboBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSplitter;
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

private:
    struct CompRow {
        QString type;
        QComboBox* versions = nullptr;
        QLabel* state = nullptr;
        QPushButton* apply = nullptr;
        QPushButton* remove = nullptr;
    };

    void buildUi();
    void runTool(const QStringList& wineArgs);
    void applyComponent(const QString& type);
    void removeComponent(const QString& type);
    void selectByName(const QString& name);
    QString currentName() const;

    PrefixManager m_mgr;
    QSplitter* m_central = nullptr;
    QListWidget* m_list = nullptr;
    QWidget* m_details = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_path = nullptr;
    QLabel* m_arch = nullptr;
    QComboBox* m_runnerBox = nullptr;
    QList<CompRow> m_rows;
    QProgressBar* m_busy = nullptr;
};
