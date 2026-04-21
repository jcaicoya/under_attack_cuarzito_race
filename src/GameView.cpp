#include "GameView.h"
#include "GameScene.h"
#include <QKeyEvent>

GameView::GameView(QWidget *parent) : QGraphicsView(parent)
{
    m_scene = new GameScene(this);
    setScene(m_scene);

    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setFrameStyle(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);
    setCacheMode(QGraphicsView::CacheNone);
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
}

void GameView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        window()->close();
        return;
    }
    if (event->isAutoRepeat()) return;
    m_scene->inputManager()->keyPressed(static_cast<Qt::Key>(event->key()));
}

void GameView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) return;
    m_scene->inputManager()->keyReleased(static_cast<Qt::Key>(event->key()));
}

void GameView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}
