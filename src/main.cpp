#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>

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

    MainWindow w;
    w.show();
    return app.exec();
}
