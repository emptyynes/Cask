#pragma once

#include "core/Config.h"
#include "core/Runner.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class NewPrefixDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewPrefixDialog(QWidget* parent = nullptr) : QDialog(parent)
    {
        setWindowTitle(tr("Новый префикс"));
        setMinimumWidth(420);
        auto* form = new QFormLayout(this);

        m_name = new QLineEdit;
        m_name->setPlaceholderText(tr("Например: Games"));
        m_runner = new QComboBox;
        const auto runners = RunnerManager::available();
        for (const Runner& r : runners)
            m_runner->addItem(r.system ? tr("Системный Wine") : r.name, r.name);
        m_arch = new QComboBox;
        m_arch->addItem(QStringLiteral("64-bit (win64)"), QStringLiteral("win64"));
        m_arch->addItem(QStringLiteral("32-bit (win32)"), QStringLiteral("win32"));

        form->addRow(tr("Имя"), m_name);
        form->addRow(tr("Wine"), m_runner);
        form->addRow(tr("Архитектура"), m_arch);

        if (runners.isEmpty()) {
            auto* hint = new QLabel(
                tr("Wine не найден. Положите сборку Wine в папку\n%1\n(внутри должен быть bin/wine) "
                   "или установите системный wine.")
                    .arg(Config::instance().runnersDir()));
            hint->setWordWrap(true);
            form->addRow(hint);
        }

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!runners.isEmpty());
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            if (!m_name->text().trimmed().isEmpty())
                accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        form->addRow(buttons);
    }

    QString prefixName() const { return m_name->text().trimmed(); }
    QString runner() const { return m_runner->currentData().toString(); }
    QString arch() const { return m_arch->currentData().toString(); }

private:
    QLineEdit* m_name = nullptr;
    QComboBox* m_runner = nullptr;
    QComboBox* m_arch = nullptr;
};
