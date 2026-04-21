#include "InputManager.h"

InputManager::InputManager(QObject *parent) : QObject(parent) {}

void InputManager::keyPressed(Qt::Key key)
{
    m_held.insert(key);
    if (key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter)
        m_confirmPressed = true;
}

void InputManager::keyReleased(Qt::Key key)
{
    m_held.remove(key);
}

bool InputManager::isMovingLeft() const
{
    return m_held.contains(Qt::Key_Left) || m_held.contains(Qt::Key_A);
}

bool InputManager::isMovingRight() const
{
    return m_held.contains(Qt::Key_Right) || m_held.contains(Qt::Key_D);
}

bool InputManager::isMovingUp() const
{
    return m_held.contains(Qt::Key_Up) || m_held.contains(Qt::Key_W);
}

bool InputManager::isMovingDown() const
{
    return m_held.contains(Qt::Key_Down) || m_held.contains(Qt::Key_S);
}

bool InputManager::isConfirmJustPressed() const
{
    return m_confirmPressed;
}

void InputManager::endFrame()
{
    m_confirmPressed = false;
}
