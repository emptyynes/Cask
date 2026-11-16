#include "ui/MainWindow.h"
#include "ui/PrefixPickerDialog.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>

#include "core/Config.h"
#include "core/DesktopIntegration.h"
#include "core/I18n.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Cask");
    app.setApplicationVersion("0.1.0");

    Config& cfg = Config::instance();
    cfg.ensureLayout();
    I18n::apply(cfg.language());  // по умолчанию "system": язык берётся из системы

    // Папку с Cask могли переместить: сверяем .desktop и mimeinfo.cache; если интеграция выключена, убираем следы.
    QString desktopErr;
    if (!DesktopIntegration::sync(cfg.desktopIntegration(), DesktopIntegration::applicationsDir(),
                                  app.applicationFilePath(), &desktopErr))
        qWarning("Desktop integration: %s", qPrintable(desktopErr));

    // Запуск файла из файлового менеджера: cask <файл>. Показываем выбор префикса и запускаем, главное окно не нужно.
    QString file;
    for (const QString& a : app.arguments().mid(1)) {
        if (!a.startsWith('-')) {
            file = a.startsWith(QLatin1String("file://")) ? QUrl(a).toLocalFile() : a;
            break;
        }
    }
    if (!file.isEmpty()) {
        const QFileInfo fi(file);
        if (!fi.isFile()) {
            QMessageBox::warning(nullptr, QStringLiteral("Cask"), QObject::tr("File not found: %1").arg(file));
            return 1;
        }
        PrefixManager mgr;
        PrefixPickerDialog picker(&mgr, fi.fileName());
        if (picker.exec() != QDialog::Accepted)
            return 0;
        QString err;
        if (!mgr.launch(picker.prefixName(), PrefixManager::programArgs(fi.absoluteFilePath()), fi.absolutePath(),
                        &err, true)) {
            QMessageBox::warning(nullptr, QStringLiteral("Cask"), err);
            return 1;
        }
        cfg.setLastPrefix(picker.prefixName());
        return 0;
    }

    MainWindow w;
    w.show();
    return app.exec();
}
