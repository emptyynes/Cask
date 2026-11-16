#pragma once

#include "core/LaunchSettings.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <algorithm>
#include <functional>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

// Editor for the prefix environment variables ("name / value" table).
class EnvDialog : public QDialog {
    Q_OBJECT
public:
    explicit EnvDialog(const QMap<QString, QString>& env, QWidget* parent = nullptr) : QDialog(parent)
    {
        setWindowTitle(tr("Environment variables"));
        resize(560, 380);
        auto* v = new QVBoxLayout(this);

        auto* hint = new QLabel(
            tr("These variables are passed to Wine and to programs started in this prefix. "
               "Variables that Cask manages itself cannot be overridden: %1. "
               "Use the Wine language setting to change the language.")
                .arg(LaunchSettings::reservedEnvNames().join(QStringLiteral(", "))));
        hint->setWordWrap(true);
        v->addWidget(hint);

        m_table = new QTableWidget(0, 2);
        m_table->setHorizontalHeaderLabels({tr("Name"), tr("Value")});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
        m_table->setColumnWidth(0, 220);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        v->addWidget(m_table, 1);

        for (auto it = env.cbegin(); it != env.cend(); ++it)
            addRow(it.key(), it.value());

        auto* rowButtons = new QHBoxLayout;
        auto* add = new QPushButton(tr("Add"));
        auto* del = new QPushButton(tr("Remove selected"));
        connect(add, &QPushButton::clicked, this, [this] {
            addRow(QString(), QString());
            m_table->setCurrentCell(m_table->rowCount() - 1, 0);
            m_table->editItem(m_table->item(m_table->rowCount() - 1, 0));
        });
        connect(del, &QPushButton::clicked, this, [this] {
            const auto rows = m_table->selectionModel()->selectedRows();
            QList<int> idx;
            for (const QModelIndex& m : rows)
                idx << m.row();
            std::sort(idx.begin(), idx.end(), std::greater<int>());
            for (int r : idx)
                m_table->removeRow(r);
        });
        rowButtons->addWidget(add);
        rowButtons->addWidget(del);
        rowButtons->addStretch(1);
        v->addLayout(rowButtons);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText(tr("OK"));
        buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            if (collect())
                accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        v->addWidget(buttons);
    }

    QMap<QString, QString> env() const { return m_result; }

private:
    void addRow(const QString& name, const QString& value)
    {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, new QTableWidgetItem(name));
        m_table->setItem(r, 1, new QTableWidgetItem(value));
    }

    bool collect()
    {
        QMap<QString, QString> out;
        for (int r = 0; r < m_table->rowCount(); ++r) {
            const QString name = m_table->item(r, 0) ? m_table->item(r, 0)->text().trimmed() : QString();
            const QString value = m_table->item(r, 1) ? m_table->item(r, 1)->text() : QString();
            if (name.isEmpty() && value.isEmpty())
                continue;
            if (name.isEmpty()) {
                fail(tr("A value has no variable name (row %1).").arg(r + 1));
                return false;
            }
            if (!LaunchSettings::isValidEnvName(name)) {
                fail(tr("Invalid variable name: %1").arg(name));
                return false;
            }
            if (LaunchSettings::isReservedEnvName(name)) {
                fail(tr("The variable %1 is set by Cask and cannot be changed.").arg(name));
                return false;
            }
            if (out.contains(name)) {
                fail(tr("The variable %1 is listed twice.").arg(name));
                return false;
            }
            out.insert(name, value);
        }
        m_result = out;
        return true;
    }

    void fail(const QString& text) { QMessageBox::warning(this, tr("Environment variables"), text); }

    QTableWidget* m_table = nullptr;
    QMap<QString, QString> m_result;
};
