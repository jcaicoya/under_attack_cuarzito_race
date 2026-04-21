#pragma once
#include <QObject>
#include <QSet>

class InputManager : public QObject {
    Q_OBJECT
public:
    explicit InputManager(QObject *parent = nullptr);

    void keyPressed(Qt::Key key);
    void keyReleased(Qt::Key key);

    bool isMovingLeft() const;
    bool isMovingRight() const;
    bool isConfirmJustPressed() const;

    void endFrame();

private:
    QSet<Qt::Key> m_held;
    bool m_confirmPressed = false;
};
