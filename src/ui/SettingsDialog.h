#pragma once

#include "core/Config.h"
#include "core/DesktopIntegration.h"
#include "core/I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCoreApplication>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr) : QDialog(parent)
    {
        setWindowTitle(tr("Settings"));
        setMinimumWidth(560);
        auto* form = new QFormLayout(this);

        m_lang = new QComboBox;
        m_lang->addItem(tr("System default"), QStringLiteral("system"));
        for (const I18n::Language& l : I18n::available())
            m_lang->addItem(l.name, l.code);
        const int li = m_lang->findData(Config::instance().language());
        m_lang->setCurrentIndex(li < 0 ? 0 : li);
        form->addRow(tr("Language"), m_lang);

        auto* root = new QLabel(Config::instance().rootDir());
        root->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(tr("Cask root"), root);

        m_prefixes = new QLineEdit(Config::instance().prefixesDir());
        m_data = new QLineEdit(Config::instance().dataDir());
        form->addRow(tr("Prefixes folder"), row(m_prefixes));
        form->addRow(tr("Data folder"), row(m_data));

        auto* hint = new QLabel(
            tr("Paths inside the Cask folder are stored as relative, so the whole folder can be "
               "moved. Existing prefixes are not moved automatically when the folder changes: "
               "move them manually."));
        hint->setWordWrap(true);
        hint->setEnabled(false);  // dimmed text: it is a note, not an option
        form->addRow(QString(), hint);

        m_desktop = new QCheckBox(tr("Integrate with the desktop"));
        m_desktop->setChecked(Config::instance().desktopIntegration());
        m_desktop->setToolTip(
            tr("Creates %1 and adds Cask to the file associations of .exe, .msi, .bat and .lnk in %2. "
               "When turned off, these entries are removed.")
                .arg(DesktopIntegration::desktopFileName(), DesktopIntegration::applicationsDir()));
        form->addRow(tr("Desktop"), m_desktop);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText(tr("OK"));
        buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            Config& c = Config::instance();
            c.setPrefixesDir(m_prefixes->text().trimmed());
            c.setDataDir(m_data->text().trimmed());
            c.ensureLayout();

            const QString lang = m_lang->currentData().toString();
            if (lang != c.language()) {
                c.setLanguage(lang);
                I18n::apply(lang);
                m_languageChanged = true;
            }
            c.setDesktopIntegration(m_desktop->isChecked());
            QString derr;
            if (!DesktopIntegration::sync(m_desktop->isChecked(), DesktopIntegration::applicationsDir(),
                                          QCoreApplication::applicationFilePath(), &derr))
                QMessageBox::warning(this, QStringLiteral("Cask"), derr);
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        form->addRow(buttons);
    }

    bool languageChanged() const { return m_languageChanged; }

private:
    QWidget* row(QLineEdit* edit)
    {
        auto* w = new QWidget;
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        auto* btn = new QPushButton(tr("Browse…"));
        connect(btn, &QPushButton::clicked, this, [this, edit] {
            const QString d = QFileDialog::getExistingDirectory(this, tr("Choose a folder"), edit->text());
            if (!d.isEmpty())
                edit->setText(d);
        });
        h->addWidget(edit, 1);
        h->addWidget(btn);
        return w;
    }

    QComboBox* m_lang = nullptr;
    QLineEdit* m_prefixes = nullptr;
    QLineEdit* m_data = nullptr;
    QCheckBox* m_desktop = nullptr;
    bool m_languageChanged = false;
};
