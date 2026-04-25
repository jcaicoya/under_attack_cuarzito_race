#include "InputManager.h"

#include <QDebug>

#ifdef Q_OS_WIN
#include "SdlControllerBackend.h"
#include "XInputControllerBackend.h"
#endif

InputManager::InputManager(QObject *parent) : QObject(parent) {}

void InputManager::keyPressed(Qt::Key key)
{
    m_held.insert(key);
    if (hasActionForKey(key)) {
        const QSet<Action> actions = actionsForKey(key);
        for (Action action : actions) {
            m_heldActions.insert(action);
            m_pressedActions.insert(action);
        }
    }
}

void InputManager::keyReleased(Qt::Key key)
{
    m_held.remove(key);
    rebuildHeldActions();
}

bool InputManager::isHeld(Action action) const
{
    return m_heldActions.contains(action) ||
           m_xInputHeldActions.contains(action) ||
           m_sdlHeldActions.contains(action);
}

bool InputManager::isJustPressed(Action action) const
{
    return m_pressedActions.contains(action);
}

bool InputManager::isMovingLeft() const
{
    return isHeld(Action::MoveLeft);
}

bool InputManager::isMovingRight() const
{
    return isHeld(Action::MoveRight);
}

bool InputManager::isMovingUp() const
{
    return isHeld(Action::MoveUp);
}

bool InputManager::isMovingDown() const
{
    return isHeld(Action::MoveDown);
}

bool InputManager::isAccelerating() const
{
    return isHeld(Action::Accelerate);
}

bool InputManager::isBraking() const
{
    return isHeld(Action::Brake);
}

bool InputManager::isConfirmJustPressed() const
{
    return isJustPressed(Action::Confirm);
}

bool InputManager::isLeftJustPressed() const
{
    return isJustPressed(Action::MoveLeft);
}

bool InputManager::isRightJustPressed() const
{
    return isJustPressed(Action::MoveRight);
}

bool InputManager::isUpJustPressed() const
{
    return isJustPressed(Action::MoveUp);
}

bool InputManager::isDownJustPressed() const
{
    return isJustPressed(Action::MoveDown);
}

void InputManager::endFrame()
{
    m_pressedActions.clear();
}

void InputManager::rebuildHeldActions()
{
    m_heldActions.clear();
    for (Qt::Key key : m_held) {
        if (!hasActionForKey(key))
            continue;

        const QSet<Action> actions = actionsForKey(key);
        for (Action action : actions)
            m_heldActions.insert(action);
    }
}

void InputManager::updateGamepad()
{
    QSet<Action> newSdlHeld;
    QSet<Action> newXInputHeld;
#ifdef Q_OS_WIN
    static bool s_logged = false;
    auto &sdl2 = SdlControllerBackend::instance();
    newSdlHeld = sdl2.poll();
    newXInputHeld = XInputControllerBackend::instance().poll();
#endif

    for (Action action : newSdlHeld) {
        if (!m_sdlHeldActions.contains(action))
            m_pressedActions.insert(action);
    }
    m_sdlHeldActions = newSdlHeld;

    for (Action action : newXInputHeld) {
        if (!m_xInputHeldActions.contains(action))
            m_pressedActions.insert(action);
    }
    m_xInputHeldActions = newXInputHeld;

#ifdef Q_OS_WIN
    if (!s_logged) {
        s_logged = true;
        qDebug().noquote() << gamepadDiagnostics();
    }
#endif
}

QString InputManager::gamepadDiagnostics() const
{
    QString report;
#ifdef Q_OS_WIN
    // SDL3 section
    report += SdlControllerBackend::instance().diagnostics();
    report += "\n";
    report += XInputControllerBackend::instance().diagnostics();
#else
    report = "Gamepad diagnostics: Windows only\n";
#endif
    return report;
}

bool InputManager::hasActionForKey(Qt::Key key)
{
    switch (key) {
    case Qt::Key_Left:
    case Qt::Key_A:
    case Qt::Key_Right:
    case Qt::Key_D:
    case Qt::Key_Up:
    case Qt::Key_W:
    case Qt::Key_Down:
    case Qt::Key_S:
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Escape:
    case Qt::Key_R:
    case Qt::Key_F11:
        return true;
    default:
        return false;
    }
}

QSet<Action> InputManager::actionsForKey(Qt::Key key)
{
    switch (key) {
    case Qt::Key_Left:
    case Qt::Key_A:
        return {Action::MoveLeft};
    case Qt::Key_Right:
    case Qt::Key_D:
        return {Action::MoveRight};
    case Qt::Key_Up:
    case Qt::Key_W:
        return {Action::MoveUp};
    case Qt::Key_Down:
    case Qt::Key_S:
        return {Action::MoveDown};
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return {Action::Confirm, Action::Accelerate};
    case Qt::Key_Shift:
    case Qt::Key_Control:
        return {Action::Brake};
    case Qt::Key_Escape:
        return {Action::Cancel};
    case Qt::Key_R:
        return {Action::Restart};
    case Qt::Key_F11:
        return {Action::Fullscreen};
    default:
        return {};
    }
}
