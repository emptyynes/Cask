#include "ui/MainWindow.h"

#include "core/Component.h"
#include "core/Config.h"
#include "core/LaunchSettings.h"
#include "core/Runner.h"
#include "ui/EnvDialog.h"
#include "ui/NewPrefixDialog.h"
#include "ui/SettingsDialog.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Cask"));
    resize(980, 740);

    m_busy = new QProgressBar;
    m_busy->setRange(0, 0);
    m_busy->setMaximumWidth(120);
    m_busy->setTextVisible(false);
    m_busy->hide();
    statusBar()->addPermanentWidget(m_busy);

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
    m_toolbar = addToolBar(tr("Main"));
    m_toolbar->setMovable(false);
    auto addAct = [&](const QString& text, void (MainWindow::*slot)()) {
        QAction* a = m_toolbar->addAction(text);
        connect(a, &QAction::triggered, this, slot);
    };
    addAct(tr("New prefix"), &MainWindow::newPrefix);
    addAct(tr("Refresh"), &MainWindow::refresh);
    addAct(tr("Settings"), &MainWindow::openSettings);

    // Центр: список слева, детали справа
    m_central = new QSplitter(this);
    m_list = new QListWidget;
    m_list->setMinimumWidth(220);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int) { onSelected(); });
    m_central->addWidget(m_list);

    m_details = new QWidget;
    auto* v = new QVBoxLayout(m_details);

    auto* info = new QGroupBox(tr("Prefix"));
    auto* form = new QFormLayout(info);
    m_name = new QLabel;
    m_path = new QLabel;
    m_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_path->setWordWrap(true);
    m_arch = new QLabel;
    m_runnerBox = new QComboBox;
    auto* runnerBtn = new QPushButton(tr("Change Wine"));
    connect(runnerBtn, &QPushButton::clicked, this, &MainWindow::applyRunner);
    auto* runnerRow = new QHBoxLayout;
    runnerRow->addWidget(m_runnerBox, 1);
    runnerRow->addWidget(runnerBtn);
    form->addRow(tr("Name"), m_name);
    form->addRow(tr("Folder"), m_path);
    form->addRow(tr("Architecture"), m_arch);
    form->addRow(tr("Wine"), runnerRow);
    v->addWidget(info);

    auto* comps = new QGroupBox(tr("Components"));
    auto* grid = new QGridLayout(comps);
    int row = 0;
    for (const auto& t : Components::types()) {
        CompRow cr;
        cr.type = t.id;
        cr.versions = new QComboBox;
        cr.apply = new QPushButton(tr("Apply"));
        cr.remove = new QPushButton(tr("Remove"));
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

    // Настройки запуска
    auto* launch = new QGroupBox(tr("Launch settings"));
    auto* lf = new QFormLayout(launch);

    m_syncBox = new QComboBox;
    m_syncBox->addItem(tr("None"), QStringLiteral("none"));
    m_syncBox->addItem(QStringLiteral("Esync"), QStringLiteral("esync"));
    m_syncBox->addItem(QStringLiteral("Fsync"), QStringLiteral("fsync"));
    m_syncBox->addItem(QStringLiteral("NTsync"), QStringLiteral("ntsync"));
    m_syncBox->setToolTip(
        tr("Synchronization primitives used by Wine. The selected mode is switched on and the "
           "others are switched off. A mode only works if your Wine build supports it."));
    m_syncWarn = new QLabel;
    m_syncWarn->setWordWrap(true);
    m_syncWarn->hide();
    auto* syncCol = new QVBoxLayout;
    syncCol->setContentsMargins(0, 0, 0, 0);
    syncCol->addWidget(m_syncBox);
    syncCol->addWidget(m_syncWarn);
    lf->addRow(tr("Synchronization"), syncCol);

    m_localeBox = new QComboBox;
    m_localeBox->setEditable(true);
    m_localeBox->setInsertPolicy(QComboBox::NoInsert);
    m_localeBox->addItem(tr("System default"), QString());
    for (const QString& code : LaunchSettings::localePresets()) {
        const QString lang = QLocale(code.section(QLatin1Char('.'), 0, 0)).nativeLanguageName();
        m_localeBox->addItem(lang.isEmpty() ? code : QStringLiteral("%1 (%2)").arg(code, lang), code);
    }
    m_localeBox->lineEdit()->setPlaceholderText(tr("e.g. de_DE.UTF-8"));
    m_localeBox->setToolTip(
        tr("Language that Wine and the programs in this prefix see (sets LANG and LC_ALL). "
           "The locale must be generated on your system. You can type your own value."));
    m_localeWarn = new QLabel;
    m_localeWarn->setWordWrap(true);
    m_localeWarn->hide();
    auto* localeCol = new QVBoxLayout;
    localeCol->setContentsMargins(0, 0, 0, 0);
    localeCol->addWidget(m_localeBox);
    localeCol->addWidget(m_localeWarn);
    lf->addRow(tr("Wine language"), localeCol);

    m_gamemode = new QCheckBox(tr("Use GameMode"));
    m_gamemode->setToolTip(LaunchSettings::gamemodeBinary().isEmpty()
                               ? tr("gamemoderun was not found. Install the 'gamemode' package.")
                               : tr("Programs started with 'Run program…' are launched through gamemoderun."));
    lf->addRow(tr("Performance"), m_gamemode);

    m_envBtn = new QPushButton;
    lf->addRow(tr("Environment"), m_envBtn);
    updateEnvButton();

    connect(m_syncBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { saveSettings(); });
    connect(m_localeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { saveSettings(); });
    connect(m_localeBox->lineEdit(), &QLineEdit::editingFinished, this, [this] { saveSettings(); });
    connect(m_gamemode, &QCheckBox::toggled, this, [this] { saveSettings(); });
    connect(m_envBtn, &QPushButton::clicked, this, &MainWindow::editEnv);
    v->addWidget(launch);

    // Действия
    auto* actions = new QHBoxLayout;
    auto* runBtn = new QPushButton(tr("Run program…"));
    connect(runBtn, &QPushButton::clicked, this, &MainWindow::runExe);
    auto* toolsBtn = new QPushButton(tr("Tools"));
    auto* menu = new QMenu(toolsBtn);
    auto addTool = [&](const QString& text, const QStringList& args) {
        QAction* a = menu->addAction(text);
        connect(a, &QAction::triggered, this, [this, args] { runTool(args); });
    };
    addTool(tr("Wine configuration (winecfg)"), {"winecfg"});
    addTool(tr("Registry editor (regedit)"), {"regedit"});
    addTool(tr("Add/remove programs"), {"uninstaller"});
    addTool(tr("Task manager"), {"taskmgr"});
    toolsBtn->setMenu(menu);
    auto* folderBtn = new QPushButton(tr("Open folder"));
    connect(folderBtn, &QPushButton::clicked, this, &MainWindow::openFolder);
    auto* delBtn = new QPushButton(tr("Delete prefix"));
    connect(delBtn, &QPushButton::clicked, this, &MainWindow::deletePrefix);
    actions->addWidget(runBtn);
    actions->addWidget(toolsBtn);
    actions->addWidget(folderBtn);
    actions->addStretch(1);
    actions->addWidget(delBtn);
    v->addLayout(actions);
    v->addStretch(1);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(m_details);
    m_central->addWidget(scroll);
    m_central->setStretchFactor(1, 1);
    setCentralWidget(m_central);
    m_central->setEnabled(!m_mgr.isBusy());
}

void MainWindow::retranslate()
{
    const QString keep = currentName();
    removeToolBar(m_toolbar);
    delete m_toolbar;
    m_toolbar = nullptr;
    m_rows.clear();
    buildUi();  // setCentralWidget удаляет прежний центральный виджет
    refresh();
    if (!keep.isEmpty())
        selectByName(keep);
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

QString MainWindow::localeValue() const
{
    // Текст совпал с пунктом списка: берём его значение, иначе это введённая вручную локаль.
    const QString text = m_localeBox->currentText().trimmed();
    const int idx = m_localeBox->findText(text, Qt::MatchFixedString);
    return idx >= 0 ? m_localeBox->itemData(idx).toString() : text;
}

void MainWindow::setLocaleValue(const QString& value)
{
    const int idx = m_localeBox->findData(value);
    if (idx >= 0)
        m_localeBox->setCurrentIndex(idx);
    else
        m_localeBox->setEditText(value);
}

void MainWindow::updateWarnings()
{
    const bool ntBad = m_syncBox->currentData().toString() == QLatin1String("ntsync") &&
                       !LaunchSettings::ntsyncAvailable();
    m_syncWarn->setText(
        tr("Warning: /dev/ntsync is missing or not accessible. NTsync needs Linux 6.14 or newer "
           "with the ntsync module and a Wine build that supports it."));
    m_syncWarn->setVisible(ntBad);

    const QString loc = localeValue();
    const bool locBad = !loc.isEmpty() && !LaunchSettings::localeInstalled(loc);
    m_localeWarn->setText(
        tr("Warning: this locale is not generated on your system (see 'locale -a'). "
           "Wine may fall back to English."));
    m_localeWarn->setVisible(locBad);
}

void MainWindow::updateEnvButton()
{
    m_envBtn->setText(m_env.isEmpty() ? tr("Edit variables…") : tr("Edit variables (%1)…").arg(m_env.size()));
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
        m_loading = true;
        m_env.clear();
        updateEnvButton();
        m_syncBox->setCurrentIndex(m_syncBox->findData(QStringLiteral("fsync")));
        setLocaleValue(QString());
        m_gamemode->setChecked(false);
        m_loading = false;
        updateWarnings();
        return;
    }
    const Prefix& p = *po;
    m_name->setText(p.name);
    m_path->setText(p.path);
    m_arch->setText(p.arch == "win64" ? "64-bit (win64)" : "32-bit (win32)");

    m_runnerBox->clear();
    for (const Runner& r : RunnerManager::available())
        m_runnerBox->addItem(r.system ? tr("System Wine") : r.name, r.name);
    int idx = m_runnerBox->findData(p.runner);
    if (idx < 0) {
        m_runnerBox->addItem(tr("%1 (not found)").arg(p.runner), p.runner);
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
            cr.versions->setToolTip(tr("Put version folders into %1").arg(Config::instance().componentsDir(cr.type)));

        const auto it = p.components.constFind(cr.type);
        if (it != p.components.constEnd()) {
            cr.state->setText(tr("Applied: %1").arg(it->version));
            const int vi = cr.versions->findText(it->version);
            if (vi >= 0)
                cr.versions->setCurrentIndex(vi);
            cr.remove->setEnabled(true);
        } else {
            cr.state->setText(tr("Not applied"));
            cr.remove->setEnabled(false);
        }
    }

    m_loading = true;
    const int si = m_syncBox->findData(p.settings.sync);
    m_syncBox->setCurrentIndex(si < 0 ? m_syncBox->findData(QStringLiteral("fsync")) : si);
    setLocaleValue(p.settings.locale);
    m_gamemode->setChecked(p.settings.gamemode);
    m_env = p.settings.env;
    updateEnvButton();
    m_loading = false;
    updateWarnings();
}

void MainWindow::saveSettings()
{
    if (m_loading)
        return;
    const QString name = currentName();
    if (name.isEmpty())
        return;

    PrefixSettings s;
    s.sync = m_syncBox->currentData().toString();
    s.locale = localeValue();
    s.gamemode = m_gamemode->isChecked();
    s.env = m_env;

    QString err;
    if (!m_mgr.setSettings(name, s, &err))
        QMessageBox::warning(this, QStringLiteral("Cask"), err);
    updateWarnings();
}

void MainWindow::editEnv()
{
    if (currentName().isEmpty())
        return;
    EnvDialog dlg(m_env, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_env = dlg.env();
    updateEnvButton();
    saveSettings();
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
    QMessageBox box(QMessageBox::Question, QStringLiteral("Cask"),
                    tr("Delete prefix '%1' with all its data? This cannot be undone.").arg(name),
                    QMessageBox::NoButton, this);
    QPushButton* yes = box.addButton(tr("Yes"), QMessageBox::YesRole);
    box.addButton(tr("No"), QMessageBox::NoRole);
    box.exec();
    if (box.clickedButton() != yes)
        return;
    QString err;
    if (!m_mgr.remove(name, &err))
        QMessageBox::warning(this, QStringLiteral("Cask"), err);
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    if (dlg.languageChanged())
        retranslate();
    else
        refresh();
}

void MainWindow::runExe()
{
    const QString name = currentName();
    if (name.isEmpty())
        return;
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Choose a program"), QString(),
        tr("Windows programs (*.exe *.msi *.bat *.lnk);;All files (*)"));
    if (file.isEmpty())
        return;
    const QFileInfo fi(file);
    const QStringList args = fi.suffix().compare("exe", Qt::CaseInsensitive) == 0
                                 ? QStringList{file}
                                 : QStringList{"start", "/unix", file};
    QString err;
    if (!m_mgr.launch(name, args, fi.absolutePath(), &err, true))
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
