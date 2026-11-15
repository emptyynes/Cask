#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>

#include "core/Config.h"
#include "core/I18n.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Cask");
    app.setApplicationVersion("0.1.0");

    Config& cfg = Config::instance();
    cfg.ensureLayout();
    I18n::apply(cfg.language());  // по умолчанию "system": язык берётся из системы

    MainWindow w;
    w.show();
    return app.exec();
}
