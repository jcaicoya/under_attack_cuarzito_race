#include "MainWindow.h"
#include "GameLaunchOptions.h"
#include "GameWidget.h"

MainWindow::MainWindow(const GameLaunchOptions &options, QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Cuarzito");
    resize(1280, 720);

    auto *game = new GameWidget(options, this);
    setCentralWidget(game);
    game->setFocus();
}
