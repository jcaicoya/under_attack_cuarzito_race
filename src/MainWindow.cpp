#include "MainWindow.h"
#include "GameWidget.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Cuarzito");
    resize(1280, 720);

    auto *game = new GameWidget(this);
    setCentralWidget(game);
    game->setFocus();
}
