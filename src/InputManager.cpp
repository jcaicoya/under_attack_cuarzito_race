#include "InputManager.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <Xinput.h>
#endif

namespace {

#ifdef Q_OS_WIN
class Sdl2ControllerBackend {
public:
    Sdl2ControllerBackend()
    {
        m_module = LoadLibraryA("SDL2.dll");
        if (!m_module)
            return;

        m_initSubSystem = load<InitSubSystemFn>("SDL_InitSubSystem");
        m_numJoysticks = load<NumJoysticksFn>("SDL_NumJoysticks");
        m_isGameController = load<IsGameControllerFn>("SDL_IsGameController");
        m_gameControllerOpen = load<GameControllerOpenFn>("SDL_GameControllerOpen");
        m_gameControllerClose = load<GameControllerCloseFn>("SDL_GameControllerClose");
        m_gameControllerUpdate = load<GameControllerUpdateFn>("SDL_GameControllerUpdate");
        m_gameControllerGetAxis = load<GameControllerGetAxisFn>("SDL_GameControllerGetAxis");
        m_gameControllerGetButton = load<GameControllerGetButtonFn>("SDL_GameControllerGetButton");

        if (!m_initSubSystem || !m_numJoysticks || !m_isGameController ||
            !m_gameControllerOpen || !m_gameControllerClose || !m_gameControllerUpdate ||
            !m_gameControllerGetAxis || !m_gameControllerGetButton) {
            return;
        }

        constexpr quint32 SDL_INIT_GAMECONTROLLER = 0x00002000u;
        m_available = m_initSubSystem(SDL_INIT_GAMECONTROLLER) == 0;
    }

    ~Sdl2ControllerBackend()
    {
        if (m_controller && m_gameControllerClose)
            m_gameControllerClose(m_controller);
    }

    QSet<Action> poll()
    {
        QSet<Action> actions;
        if (!m_available)
            return actions;

        ensureController();
        if (!m_controller)
            return actions;

        m_gameControllerUpdate();

        constexpr int AXIS_LEFT_X = 0;
        constexpr int AXIS_LEFT_Y = 1;
        constexpr int AXIS_TRIGGER_LEFT = 4;
        constexpr int AXIS_TRIGGER_RIGHT = 5;
        constexpr int BUTTON_A = 0;
        constexpr int BUTTON_B = 1;
        constexpr int BUTTON_BACK = 4;
        constexpr int BUTTON_START = 6;
        constexpr int BUTTON_DPAD_UP = 11;
        constexpr int BUTTON_DPAD_DOWN = 12;
        constexpr int BUTTON_DPAD_LEFT = 13;
        constexpr int BUTTON_DPAD_RIGHT = 14;
        constexpr qint16 DEAD_ZONE = 9000;

        const qint16 lx = m_gameControllerGetAxis(m_controller, AXIS_LEFT_X);
        const qint16 ly = m_gameControllerGetAxis(m_controller, AXIS_LEFT_Y);
        const qint16 lt = m_gameControllerGetAxis(m_controller, AXIS_TRIGGER_LEFT);
        const qint16 rt = m_gameControllerGetAxis(m_controller, AXIS_TRIGGER_RIGHT);

        if (m_gameControllerGetButton(m_controller, BUTTON_DPAD_LEFT) || lx < -DEAD_ZONE)
            actions.insert(Action::MoveLeft);
        if (m_gameControllerGetButton(m_controller, BUTTON_DPAD_RIGHT) || lx > DEAD_ZONE)
            actions.insert(Action::MoveRight);
        if (m_gameControllerGetButton(m_controller, BUTTON_DPAD_UP) || ly < -DEAD_ZONE)
            actions.insert(Action::MoveUp);
        if (m_gameControllerGetButton(m_controller, BUTTON_DPAD_DOWN) || ly > DEAD_ZONE)
            actions.insert(Action::MoveDown);
        if (m_gameControllerGetButton(m_controller, BUTTON_A) ||
            m_gameControllerGetButton(m_controller, BUTTON_START)) {
            actions.insert(Action::Confirm);
        }
        if (m_gameControllerGetButton(m_controller, BUTTON_A) || rt > DEAD_ZONE)
            actions.insert(Action::Accelerate);
        if (m_gameControllerGetButton(m_controller, BUTTON_B) ||
            m_gameControllerGetButton(m_controller, BUTTON_BACK)) {
            actions.insert(Action::Cancel);
        }
        if (m_gameControllerGetButton(m_controller, BUTTON_B) || lt > DEAD_ZONE)
            actions.insert(Action::Brake);

        return actions;
    }

private:
    using InitSubSystemFn = int (__cdecl *)(quint32);
    using NumJoysticksFn = int (__cdecl *)();
    using IsGameControllerFn = int (__cdecl *)(int);
    using GameControllerOpenFn = void *(__cdecl *)(int);
    using GameControllerCloseFn = void (__cdecl *)(void *);
    using GameControllerUpdateFn = void (__cdecl *)();
    using GameControllerGetAxisFn = qint16 (__cdecl *)(void *, int);
    using GameControllerGetButtonFn = quint8 (__cdecl *)(void *, int);

    template <typename Fn>
    Fn load(const char *name) const
    {
        return reinterpret_cast<Fn>(GetProcAddress(m_module, name));
    }

    void ensureController()
    {
        if (m_controller)
            return;

        const int count = m_numJoysticks();
        for (int i = 0; i < count; ++i) {
            if (!m_isGameController(i))
                continue;

            m_controller = m_gameControllerOpen(i);
            if (m_controller)
                return;
        }
    }

    HMODULE m_module = nullptr;
    void *m_controller = nullptr;
    bool m_available = false;
    InitSubSystemFn m_initSubSystem = nullptr;
    NumJoysticksFn m_numJoysticks = nullptr;
    IsGameControllerFn m_isGameController = nullptr;
    GameControllerOpenFn m_gameControllerOpen = nullptr;
    GameControllerCloseFn m_gameControllerClose = nullptr;
    GameControllerUpdateFn m_gameControllerUpdate = nullptr;
    GameControllerGetAxisFn m_gameControllerGetAxis = nullptr;
    GameControllerGetButtonFn m_gameControllerGetButton = nullptr;
};
#endif

} // namespace

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
           m_gamepadHeldActions.contains(action) ||
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
#ifdef Q_OS_WIN
    static Sdl2ControllerBackend sdl2Backend;
    newSdlHeld = sdl2Backend.poll();
#endif

    for (Action action : newSdlHeld) {
        if (!m_sdlHeldActions.contains(action))
            m_pressedActions.insert(action);
    }
    m_sdlHeldActions = newSdlHeld;

#ifdef Q_OS_WIN
    using XInputGetStateFn = DWORD (WINAPI *)(DWORD, XINPUT_STATE *);

    static HMODULE module = []() -> HMODULE {
        const char *dlls[] = {"xinput1_4.dll", "xinput9_1_0.dll", "xinput1_3.dll"};
        for (const char *dll : dlls) {
            if (HMODULE handle = LoadLibraryA(dll))
                return handle;
        }
        return nullptr;
    }();

    static XInputGetStateFn getState = module
        ? reinterpret_cast<XInputGetStateFn>(GetProcAddress(module, "XInputGetState"))
        : nullptr;

    QSet<Action> newHeld;
    if (getState) {
        XINPUT_STATE state = {};
        for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
            if (getState(index, &state) != ERROR_SUCCESS)
                continue;

            const WORD buttons = state.Gamepad.wButtons;
            const SHORT lx = state.Gamepad.sThumbLX;
            const SHORT ly = state.Gamepad.sThumbLY;
            constexpr SHORT deadZone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;

            if ((buttons & XINPUT_GAMEPAD_DPAD_LEFT) || lx < -deadZone)
                newHeld.insert(Action::MoveLeft);
            if ((buttons & XINPUT_GAMEPAD_DPAD_RIGHT) || lx > deadZone)
                newHeld.insert(Action::MoveRight);
            if ((buttons & XINPUT_GAMEPAD_DPAD_UP) || ly > deadZone)
                newHeld.insert(Action::MoveUp);
            if ((buttons & XINPUT_GAMEPAD_DPAD_DOWN) || ly < -deadZone)
                newHeld.insert(Action::MoveDown);
            if ((buttons & XINPUT_GAMEPAD_A) || (buttons & XINPUT_GAMEPAD_START))
                newHeld.insert(Action::Confirm);
            if ((buttons & XINPUT_GAMEPAD_A) || state.Gamepad.bRightTrigger > 30)
                newHeld.insert(Action::Accelerate);
            if ((buttons & XINPUT_GAMEPAD_B) || (buttons & XINPUT_GAMEPAD_BACK))
                newHeld.insert(Action::Cancel);
            if ((buttons & XINPUT_GAMEPAD_B) || state.Gamepad.bLeftTrigger > 30)
                newHeld.insert(Action::Brake);

            break;
        }
    }

    for (Action action : newHeld) {
        if (!m_gamepadHeldActions.contains(action))
            m_pressedActions.insert(action);
    }
    m_gamepadHeldActions = newHeld;
#endif
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
    case Qt::Key_F11:
        return {Action::Fullscreen};
    default:
        return {};
    }
}
