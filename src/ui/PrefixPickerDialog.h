#pragma once

#include "core/Config.h"
#include "core/PrefixManager.h"
#include "ui/NewPrefixDialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

// Выбор префикса для запуска файла (.exe, .msi, ...), открытого двойным кликом в файловом менеджере.
// Обычные виджеты Qt без своих стилей. Двойной клик по префиксу = «Выбрать».
class PrefixPickerDialog : public QDialog {
    Q_OBJECT
public:
    PrefixPickerDialog(PrefixManager* mgr, const QString& fileName, QWidget* parent = nullptr)
        : QDialog(parent), m_mgr(mgr)
    {
        setWindowTitle(tr("Choose a prefix"));
        setMinimumSize(380, 360);
        auto* v = new QVBoxLayout(this);

        m_infoText = tr("Run “%1” in the prefix:").arg(fileName);
        m_info = new QLabel(m_infoText);
        m_info->setWordWrap(true);
        v->addWidget(m_info);

        m_list = new QListWidget;
        v->addWidget(m_list, 1);

        m_create = new QPushButton(tr("Create new prefix"));
        v->addWidget(m_create);

        m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Select"));
        m_buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
        v->addWidget(m_buttons);

        connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(m_list, &QListWidget::itemDoubleClicked, this, [this] { accept(); });
        connect(m_list, &QListWidget::currentRowChanged, this, [this] { updateButtons(); });
        connect(m_create, &QPushButton::clicked, this, &PrefixPickerDialog::createPrefix);
        connect(m_mgr, &PrefixManager::busyChanged, this, [this](bool busy) {
            if (!busy)
                finishCreate();
        });

        fill(Config::instance().lastPrefix());
    }

    QString prefixName() const
    {
        const auto* it = m_list->currentItem();
        return it ? it->text() : QString();
    }

    void accept() override
    {
        if (m_pending.isEmpty() && !prefixName().isEmpty())
            QDialog::accept();
    }

private:
    void fill(const QString& select)
    {
        m_list->clear();
        const auto prefixes = m_mgr->list();
        for (const Prefix& p : prefixes)
            m_list->addItem(p.name);
        int row = 0;
        for (int i = 0; i < m_list->count(); ++i)
            if (m_list->item(i)->text() == select)
                row = i;
        if (m_list->count() > 0)
            m_list->setCurrentRow(row);
        updateButtons();
    }

    void updateButtons()
    {
        m_buttons->button(QDialogButtonBox::Ok)->setEnabled(m_pending.isEmpty() && m_list->currentRow() >= 0);
    }

    void createPrefix()
    {
        NewPrefixDialog dlg(this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        QString err;
        if (!m_mgr->create(dlg.prefixName(), dlg.runner(), dlg.arch(), &err)) {
            QMessageBox::warning(this, QStringLiteral("Cask"), err);
            return;
        }
        // wineboot --init идёт в фоне: до его конца префикс выбрать нельзя.
        m_pending = dlg.prefixName();
        m_list->setEnabled(false);
        m_create->setEnabled(false);
        m_info->setText(tr("Creating prefix…"));
        updateButtons();
    }

    void finishCreate()
    {
        if (m_pending.isEmpty())
            return;
        const QString name = m_pending;
        m_pending.clear();
        m_list->setEnabled(true);
        m_create->setEnabled(true);
        m_info->setText(m_infoText);
        fill(name);
    }

    PrefixManager* m_mgr;
    QLabel* m_info = nullptr;
    QListWidget* m_list = nullptr;
    QPushButton* m_create = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QString m_pending;   // имя создаваемого префикса, пока идёт wineboot
    QString m_infoText;
};
