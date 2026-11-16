#pragma once

#include "core/Config.h"
#include "core/PrefixManager.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

// Выбор префикса для запуска файла (.exe, .msi, ...), открытого двойным кликом в файловом менеджере.
// Обычные виджеты Qt без своих стилей. Двойной клик по префиксу = «Выбрать».
// Префиксы создаются в главном окне.
class PrefixPickerDialog : public QDialog {
    Q_OBJECT
public:
    PrefixPickerDialog(PrefixManager* mgr, const QString& fileName, QWidget* parent = nullptr) : QDialog(parent)
    {
        setWindowTitle(tr("Choose a prefix"));
        setMinimumSize(380, 340);
        auto* v = new QVBoxLayout(this);

        const auto prefixes = mgr->list();
        auto* info = new QLabel(prefixes.isEmpty()
                                    ? tr("There are no prefixes yet. Create one in the main Cask window.")
                                    : tr("Run “%1” in the prefix:").arg(fileName));
        info->setWordWrap(true);
        v->addWidget(info);

        m_list = new QListWidget;
        const QString last = Config::instance().lastPrefix();
        int row = 0;
        for (int i = 0; i < prefixes.size(); ++i) {
            m_list->addItem(prefixes[i].name);
            if (prefixes[i].name == last)
                row = i;
        }
        if (m_list->count() > 0)
            m_list->setCurrentRow(row);
        v->addWidget(m_list, 1);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText(tr("Select"));
        buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
        auto* ok = buttons->button(QDialogButtonBox::Ok);
        ok->setEnabled(m_list->currentRow() >= 0);
        v->addWidget(buttons);

        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(m_list, &QListWidget::currentRowChanged, this, [ok](int r) { ok->setEnabled(r >= 0); });
        connect(m_list, &QListWidget::itemDoubleClicked, this, [this] { accept(); });
    }

    QString prefixName() const
    {
        const auto* it = m_list->currentItem();
        return it ? it->text() : QString();
    }

    void accept() override
    {
        if (!prefixName().isEmpty())
            QDialog::accept();
    }

private:
    QListWidget* m_list = nullptr;
};
