#pragma once

#include <QElapsedTimer>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QTimer>
#include "CaveRenderer.h"

class GameScene;
struct GameLaunchOptions;

class GameWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit GameWidget(const GameLaunchOptions &options, QWidget *parent = nullptr);
    ~GameWidget() override;

protected:
    void initializeGL() override;
    void paintGL() override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void tick();

private:
    CaveRenderer m_caveRenderer;
    GameScene *m_scene = nullptr;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastMs = 0;
};
