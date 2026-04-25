#pragma once
#include <QObject>
#include <QSet>

#include "InputAction.h"

class InputManager : public QObject {
    Q_OBJECT
public:
    explicit InputManager(QObject *parent = nullptr);

    void keyPressed(Qt::Key key);
    void keyReleased(Qt::Key key);
    void updateGamepad();

    bool isHeld(Action action) const;
    bool isJustPressed(Action action) const;

    bool isMovingLeft()  const;
    bool isMovingRight() const;
    bool isMovingUp()    const;
    bool isMovingDown()  const;
    bool isAccelerating() const;
    bool isBraking() const;
    bool isLeftJustPressed() const;
    bool isRightJustPressed() const;
    bool isUpJustPressed() const;
    bool isDownJustPressed() const;
    bool isConfirmJustPressed() const;

    void endFrame();

    // Returns a multi-line diagnostic report: SDL load status, controller
    // count, names, GUIDs, and XInput status.  Call after updateGamepad().
    QString gamepadDiagnostics() const;

private:
    void rebuildHeldActions();

    QSet<Qt::Key> m_held;
    QSet<Action> m_heldActions;
    QSet<Action> m_xInputHeldActions;
    QSet<Action> m_sdlHeldActions;
    QSet<Action> m_pressedActions;
};
