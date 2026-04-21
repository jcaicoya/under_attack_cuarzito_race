#include <QApplication>
#include <csignal>
#include "MainWindow.h"

static void handleSignal(int) { QApplication::quit(); }

int main(int argc, char *argv[])
{
    std::signal(SIGINT,  handleSignal);
    std::signal(SIGTERM, handleSignal);

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
