#include "InputManager.h"

#include <QDebug>

#include "KeyboardActionMap.h"

#ifdef Q_OS_WIN
#include "SdlControllerBackend.h"
#include "XInputControllerBackend.h"
#endif

InputManager::InputManager(QObject *parent) : QObject(parent) {}

void InputManager::keyPressed(Qt::Key key)
{
    m_held.insert(key);
    if (KeyboardActionMap::hasActionForKey(key)) {
        const QSet<Action> actions = KeyboardActionMap::actionsForKey(key);
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
        if (!KeyboardActionMap::hasActionForKey(key))
            continue;

        const QSet<Action> actions = KeyboardActionMap::actionsForKey(key);
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
