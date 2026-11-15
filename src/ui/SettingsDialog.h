#pragma once

#include "core/Config.h"

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
        setWindowTitle(tr("Настройки"));
        setMinimumWidth(560);
        auto* form = new QFormLayout(this);

        auto* root = new QLabel(Config::instance().rootDir());
        root->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(tr("Корень Cask"), root);

        m_prefixes = new QLineEdit(Config::instance().prefixesDir());
        m_data = new QLineEdit(Config::instance().dataDir());
        form->addRow(tr("Папка префиксов"), row(m_prefixes));
        form->addRow(tr("Папка данных"), row(m_data));

        auto* hint = new QLabel(
            tr("Пути внутри папки Cask сохраняются относительными, поэтому программу можно "
               "переносить целиком. Существующие префиксы при смене папки не переносятся "
               "автоматически: переместите их вручную."));
        hint->setWordWrap(true);
        form->addRow(hint);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            Config& c = Config::instance();
            c.setPrefixesDir(m_prefixes->text().trimmed());
            c.setDataDir(m_data->text().trimmed());
            c.ensureLayout();
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        form->addRow(buttons);
    }

private:
    QWidget* row(QLineEdit* edit)
    {
        auto* w = new QWidget;
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        auto* btn = new QPushButton(tr("Обзор…"));
        connect(btn, &QPushButton::clicked, this, [this, edit] {
            const QString d = QFileDialog::getExistingDirectory(this, tr("Выберите папку"), edit->text());
            if (!d.isEmpty())
                edit->setText(d);
        });
        h->addWidget(edit, 1);
        h->addWidget(btn);
        return w;
    }

    QLineEdit* m_prefixes = nullptr;
    QLineEdit* m_data = nullptr;
};
