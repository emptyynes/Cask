#include "ui/MainWindow.h"

#include "core/Component.h"
#include "core/Config.h"
#include "core/Runner.h"
#include "ui/NewPrefixDialog.h"
#include "ui/SettingsDialog.h"

#include <QAction>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Cask"));
    resize(960, 600);
    buildUi();

    connect(&m_mgr, &PrefixManager::changed, this, &MainWindow::refresh);
    connect(&m_mgr, &PrefixManager::busyChanged, this, [this](bool busy) {
        m_busy->setVisible(busy);
        m_central->setEnabled(!busy);
    });
    connect(&m_mgr, &PrefixManager::message, this, [this](const QString& text, bool ok) {
        statusBar()->showMessage(text, 8000);
        if (!ok)
            QMessageBox::warning(this, QStringLiteral("Cask"), text);
    });

    refresh();
}

void MainWindow::buildUi()
{
    // Тулбар
    auto* tb = addToolBar(tr("Главная"));
    tb->setMovable(false);
    auto addAct = [&](const QString& text, void (MainWindow::*slot)()) {
        QAction* a = tb->addAction(text);
        connect(a, &QAction::triggered, this, slot);
    };
    addAct(tr("Новый префикс"), &MainWindow::newPrefix);
    addAct(tr("Обновить"), &MainWindow::refresh);
    addAct(tr("Настройки"), &MainWindow::openSettings);

    // Центр: список слева, детали справа
    m_central = new QSplitter(this);
    m_list = new QListWidget;
    m_list->setMinimumWidth(220);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int) { onSelected(); });
    m_central->addWidget(m_list);

    m_details = new QWidget;
    auto* v = new QVBoxLayout(m_details);

    auto* info = new QGroupBox(tr("Префикс"));
    auto* form = new QFormLayout(info);
    m_name = new QLabel;
    m_path = new QLabel;
    m_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_path->setWordWrap(true);
    m_arch = new QLabel;
    m_runnerBox = new QComboBox;
    auto* runnerBtn = new QPushButton(tr("Сменить Wine"));
    connect(runnerBtn, &QPushButton::clicked, this, &MainWindow::applyRunner);
    auto* runnerRow = new QHBoxLayout;
    runnerRow->addWidget(m_runnerBox, 1);
    runnerRow->addWidget(runnerBtn);
    form->addRow(tr("Имя"), m_name);
    form->addRow(tr("Папка"), m_path);
    form->addRow(tr("Архитектура"), m_arch);
    form->addRow(tr("Wine"), runnerRow);
    v->addWidget(info);

    auto* comps = new QGroupBox(tr("Компоненты"));
    auto* grid = new QGridLayout(comps);
    int row = 0;
    for (const auto& t : Components::types()) {
        CompRow cr;
        cr.type = t.id;
        cr.versions = new QComboBox;
        cr.apply = new QPushButton(tr("Применить"));
        cr.remove = new QPushButton(tr("Удалить"));
        cr.state = new QLabel;
        grid->addWidget(new QLabel(t.title), row, 0);
        grid->addWidget(cr.versions, row, 1);
        grid->addWidget(cr.apply, row, 2);
        grid->addWidget(cr.remove, row, 3);
        grid->addWidget(cr.state, row, 4);
        const QString type = t.id;
        connect(cr.apply, &QPushButton::clicked, this, [this, type] { applyComponent(type); });
        connect(cr.remove, &QPushButton::clicked, this, [this, type] { removeComponent(type); });
        m_rows.append(cr);
        ++row;
    }
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(4, 1);
    v->addWidget(comps);

    // Действия
    auto* actions = new QHBoxLayout;
    auto* runBtn = new QPushButton(tr("Запустить программу…"));
    connect(runBtn, &QPushButton::clicked, this, &MainWindow::runExe);
    auto* toolsBtn = new QPushButton(tr("Инструменты"));
    auto* menu = new QMenu(toolsBtn);
    auto addTool = [&](const QString& text, const QStringList& args) {
        QAction* a = menu->addAction(text);
        connect(a, &QAction::triggered, this, [this, args] { runTool(args); });
    };
    addTool(tr("Настройка Wine (winecfg)"), {"winecfg"});
    addTool(tr("Редактор реестра (regedit)"), {"regedit"});
    addTool(tr("Установка и удаление программ"), {"uninstaller"});
    addTool(tr("Диспетчер задач"), {"taskmgr"});
    toolsBtn->setMenu(menu);
    auto* folderBtn = new QPushButton(tr("Открыть папку"));
    connect(folderBtn, &QPushButton::clicked, this, &MainWindow::openFolder);
    auto* delBtn = new QPushButton(tr("Удалить префикс"));
    connect(delBtn, &QPushButton::clicked, this, &MainWindow::deletePrefix);
    actions->addWidget(runBtn);
    actions->addWidget(toolsBtn);
    actions->addWidget(folderBtn);
    actions->addStretch(1);
    actions->addWidget(delBtn);
    v->addLayout(actions);
    v->addStretch(1);

    m_central->addWidget(m_details);
    m_central->setStretchFactor(1, 1);
    setCentralWidget(m_central);

    m_busy = new QProgressBar;
    m_busy->setRange(0, 0);
    m_busy->setMaximumWidth(120);
    m_busy->setTextVisible(false);
    m_busy->hide();
    statusBar()->addPermanentWidget(m_busy);
}

QString MainWindow::currentName() const
{
    const QListWidgetItem* it = m_list->currentItem();
    return it ? it->data(Qt::UserRole).toString() : QString();
}

void MainWindow::selectByName(const QString& name)
{
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == name) {
            m_list->setCurrentRow(i);
            return;
        }
    }
}

void MainWindow::refresh()
{
    const QString keep = currentName();
    {
        const QSignalBlocker blocker(m_list);
        m_list->clear();
        for (const Prefix& p : m_mgr.list()) {
            auto* it = new QListWidgetItem(p.name);
            it->setData(Qt::UserRole, p.name);
            m_list->addItem(it);
        }
        if (!keep.isEmpty())
            selectByName(keep);
        if (!m_list->currentItem() && m_list->count() > 0)
            m_list->setCurrentRow(0);
    }
    onSelected();
}

void MainWindow::onSelected()
{
    const QString name = currentName();
    const auto po = m_mgr.find(name);
    m_details->setEnabled(po.has_value());
    if (!po) {
        m_name->clear();
        m_path->clear();
        m_arch->clear();
        m_runnerBox->clear();
        for (CompRow& cr : m_rows) {
            cr.versions->clear();
            cr.state->clear();
        }
        return;
    }
    const Prefix& p = *po;
    m_name->setText(p.name);
    m_path->setText(p.path);
    m_arch->setText(p.arch == "win64" ? "64-bit (win64)" : "32-bit (win32)");

    m_runnerBox->clear();
    for (const Runner& r : RunnerManager::available())
        m_runnerBox->addItem(r.system ? tr("Системный Wine") : r.name, r.name);
    int idx = m_runnerBox->findData(p.runner);
    if (idx < 0) {
        m_runnerBox->addItem(tr("%1 (не найден)").arg(p.runner), p.runner);
        idx = m_runnerBox->count() - 1;
    }
    m_runnerBox->setCurrentIndex(idx);

    for (CompRow& cr : m_rows) {
        const QStringList vers = Components::versions(cr.type);
        cr.versions->clear();
        cr.versions->addItems(vers);
        cr.versions->setEnabled(!vers.isEmpty());
        cr.apply->setEnabled(!vers.isEmpty());
        if (vers.isEmpty())
            cr.versions->setToolTip(tr("Положите папки версий в %1").arg(Config::instance().componentsDir(cr.type)));

        const auto it = p.components.constFind(cr.type);
        if (it != p.components.constEnd()) {
            cr.state->setText(tr("Применён: %1").arg(it->version));
            const int vi = cr.versions->findText(it->version);
            if (vi >= 0)
                cr.versions->setCurrentIndex(vi);
            cr.remove->setEnabled(true);
        } else {
            cr.state->setText(tr("Не применён"));
            cr.remove->setEnabled(false);
        }
    }
}

void MainWindow::newPrefix()
{
    NewPrefixDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    QString err;
    if (!m_mgr.create(dlg.prefixName(), dlg.runner(), dlg.arch(), &err)) {
        QMessageBox::warning(this, QStringLiteral("Cask"), err);
        return;
    }
    selectByName(dlg.prefixName());
}

void MainWindow::deletePrefix()
{
    const QString name = currentName();
    if (name.isEmpty())
        return;
    const auto ans = QMessageBox::question(
        this, QStringLiteral("Cask"),
        tr("Удалить префикс «%1» вместе со всеми данными? Это нельзя отменить.").arg(name));
    if (ans != QMessageBox::Yes)
        return;
    QString err;
    if (!m_mgr.remove(name, &err))
        QMessageBox::warning(this, QStringLiteral("Cask"), err);
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted)
        refresh();
}

void MainWindow::runExe()
{
    const QString name = currentName();
    if (name.isEmpty())
        return;
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Выберите программу"), QString(),
        tr("Программы Windows (*.exe *.msi *.bat *.lnk);;Все файлы (*)"));
    if (file.isEmpty())
        return;
    const QFileInfo fi(file);
    const QStringList args = fi.suffix().compare("exe", Qt::CaseInsensitive) == 0
                                 ? QStringList{file}
                                 : QStringList{"start", "/unix", file};
    QString err;
    if (!m_mgr.launch(name, args, fi.absolutePath(), &err))
        QMessageBox::warning(this, QStringLiteral("Cask"), err);
}

void MainWindow::runTool(const QStringList& wineArgs)
{
    const QString name = currentName();
    if (name.isEmpty())
        return;
    QString err;
    if (!m_mgr.launch(name, wineArgs, QString(), &err))
        QMessageBox::warning(this, QStringLiteral("Cask"), err);
}

void MainWindow::openFolder()
{
    const auto p = m_mgr.find(currentName());
    if (p)
        QDesktopServices::openUrl(QUrl::fromLocalFile(p->path));
}

void MainWindow::applyRunner()
{
    const QString name = currentName();
    const QString runner = m_runnerBox->currentData().toString();
    if (name.isEmpty() || runner.isEmpty())
        return;
    const auto p = m_mgr.find(name);
    if (p && p->runner == runner)
        return;
    QString err;
    if (!m_mgr.setRunner(name, runner, &err))
        QMessageBox::warning(this, QStringLiteral("Cask"), err);
}

void MainWindow::applyComponent(const QString& type)
{
    const QString name = currentName();
    for (const CompRow& cr : std::as_const(m_rows)) {
        if (cr.type != type)
            continue;
        const QString ver = cr.versions->currentText();
        if (name.isEmpty() || ver.isEmpty())
            return;
        QString err;
        if (!m_mgr.applyComponent(name, type, ver, &err))
            QMessageBox::warning(this, QStringLiteral("Cask"), err);
        return;
    }
}

void MainWindow::removeComponent(const QString& type)
{
    const QString name = currentName();
    if (name.isEmpty())
        return;
    QString err;
    if (!m_mgr.removeComponent(name, type, &err))
        QMessageBox::warning(this, QStringLiteral("Cask"), err);
}
