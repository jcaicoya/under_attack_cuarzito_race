#pragma once
#include <QMainWindow>

struct GameLaunchOptions;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const GameLaunchOptions &options, QWidget *parent = nullptr);
};
