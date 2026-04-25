#pragma once

#include <QHash>

#include <cstddef>

enum class Action {
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    Accelerate,
    Brake,
    Confirm,
    Cancel,
    Restart,
    Fullscreen,
};

inline size_t qHash(Action action, size_t seed = 0) noexcept
{
    return ::qHash(static_cast<int>(action), seed);
}
