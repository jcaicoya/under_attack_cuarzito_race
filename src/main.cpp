#include <QApplication>
#include <QSurfaceFormat>
#include <csignal>
#include "MainWindow.h"

static void handleSignal(int) { QApplication::quit(); }

int main(int argc, char *argv[])
{
    std::signal(SIGINT,  handleSignal);
    std::signal(SIGTERM, handleSignal);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication a(argc, argv);
    MainWindow w;
    w.showFullScreen();
    return a.exec();
}
