#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>

#include "core/Config.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Cask");
    app.setApplicationVersion("0.1.0");

    Config::instance().ensureLayout();

    MainWindow w;
    w.show();
    return app.exec();
}
