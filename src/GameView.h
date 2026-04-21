#pragma once
#include <QGraphicsView>

class GameScene;

class GameView : public QGraphicsView {
    Q_OBJECT
public:
    explicit GameView(QWidget *parent = nullptr);

    GameScene *gameScene() const { return m_scene; }

protected:
    void keyPressEvent(QKeyEvent *event)   override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event)  override;

private:
    GameScene *m_scene = nullptr;
};
