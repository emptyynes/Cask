#pragma once

#include "core/Config.h"
#include "core/I18n.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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
        form->addRow(hint);

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
    bool m_languageChanged = false;
};
