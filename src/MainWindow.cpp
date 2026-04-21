#include "MainWindow.h"
#include "GameView.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Cuarzito");
    resize(1280, 720);

    auto *view = new GameView(this);
    setCentralWidget(view);
    view->setFocus();
}
