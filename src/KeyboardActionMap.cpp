#include "KeyboardActionMap.h"

namespace KeyboardActionMap {

bool hasActionForKey(Qt::Key key)
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

QSet<Action> actionsForKey(Qt::Key key)
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

} // namespace KeyboardActionMap
