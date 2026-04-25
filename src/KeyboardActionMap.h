#pragma once

#include <QSet>
#include <Qt>

#include "InputAction.h"

namespace KeyboardActionMap {

bool hasActionForKey(Qt::Key key);
QSet<Action> actionsForKey(Qt::Key key);

} // namespace KeyboardActionMap
