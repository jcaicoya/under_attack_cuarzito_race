#include "InputManager.h"

#include <QDebug>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <Xinput.h>
#endif

namespace {

#ifdef Q_OS_WIN
// SDL3 controller backend — loads SDL3.dll dynamically at runtime.
// SDL3 uses instance IDs for joystick enumeration and returns bool (1 byte)
// from init/query functions; we mask the low byte when reading as int.
class SdlControllerBackend {
public:
    static SdlControllerBackend &instance()
    {
        static SdlControllerBackend inst;
        return inst;
    }

    const QString &diagnostics() const { return m_diag; }

    QSet<Action> poll()
    {
        QSet<Action> actions;
        if (!m_available) return actions;

        ensureController();
        if (!m_controller) return actions;

        if (m_update) m_update();

        constexpr int    AX_LX = 0, AX_LY = 1, AX_LT = 4, AX_RT = 5;
        constexpr int    BTN_A = 0, BTN_B = 1, BTN_BACK = 4, BTN_START = 6;
        constexpr int    BTN_DU = 11, BTN_DD = 12, BTN_DL = 13, BTN_DR = 14;
        constexpr qint16 DEAD  = 9000;

        const qint16 lx = m_getAxis(m_controller, AX_LX);
        const qint16 ly = m_getAxis(m_controller, AX_LY);
        const qint16 lt = m_getAxis(m_controller, AX_LT);
        const qint16 rt = m_getAxis(m_controller, AX_RT);
        // SDL3 bool returns: mask low byte — upper bytes are not guaranteed zero.
        auto btn = [&](int b){ return (m_getButton(m_controller, b) & 0xFF) != 0; };

        if (btn(BTN_DL) || lx < -DEAD) actions.insert(Action::MoveLeft);
        if (btn(BTN_DR) || lx >  DEAD) actions.insert(Action::MoveRight);
        if (btn(BTN_DU) || ly < -DEAD) actions.insert(Action::MoveUp);
        if (btn(BTN_DD) || ly >  DEAD) actions.insert(Action::MoveDown);
        if (btn(BTN_A)  || btn(BTN_START)) actions.insert(Action::Confirm);
        if (btn(BTN_A)  || rt > DEAD)  actions.insert(Action::Accelerate);
        if (btn(BTN_B)  || btn(BTN_BACK)) actions.insert(Action::Cancel);
        if (btn(BTN_B)  || lt > DEAD)  actions.insert(Action::Brake);

        return actions;
    }

private:
    // SDL_JoystickID = Uint32
    using InitFn          = int         (__cdecl *)(quint32);
    using UpdateFn        = void        (__cdecl *)();
    using CloseFn         = void        (__cdecl *)(void *);
    using GetAxisFn       = qint16      (__cdecl *)(void *, int);
    using GetButtonFn     = int         (__cdecl *)(void *, int);
    using GetNameFn       = const char *(__cdecl *)(void *);
    using OpenFn          = void *      (__cdecl *)(quint32);
    using GetJoysticksFn  = quint32 *   (__cdecl *)(int *);
    using IsGamepadFn     = int         (__cdecl *)(quint32);
    using JoyNameByIDFn   = const char *(__cdecl *)(quint32);
    using SdlFreeFn       = void        (__cdecl *)(void *);

    static QString regString(HKEY root, const char *subKey, const char *value)
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(root, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            return {};
        char buf[MAX_PATH] = {};
        DWORD size = MAX_PATH, type = 0;
        const bool ok = RegQueryValueExA(hKey, value, nullptr, &type,
                                         reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS;
        RegCloseKey(hKey);
        return ok ? QString::fromLocal8Bit(buf) : QString{};
    }

    template <typename Fn>
    Fn load(const char *name) const
    {
        return reinterpret_cast<Fn>(GetProcAddress(m_module, name));
    }

    SdlControllerBackend()
    {
        m_diag += "=== SDL3 Controller Diagnostics ===\n";

        // Find Steam's actual install path via registry
        const QString steam1 = regString(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\WOW6432Node\\Valve\\Steam", "InstallPath");
        const QString steam2 = regString(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Valve\\Steam", "InstallPath");
        const QString steam3 = regString(HKEY_CURRENT_USER,
            "SOFTWARE\\Valve\\Steam", "SteamPath");

        char exeDir[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
        if (char *sl = strrchr(exeDir, '\\')) *(sl + 1) = '\0';

        QStringList candidates;
        auto addDir = [&](const QString &dir) {
            if (dir.isEmpty()) return;
            const QString d = dir.endsWith('\\') ? dir : dir + "\\";
            candidates << d + "SDL3.dll";
        };
        addDir(QString::fromLocal8Bit(exeDir));
        candidates << "SDL3.dll";   // PATH lookup
        addDir(steam1);
        addDir(steam2);
        addDir(steam3);
        addDir("C:\\Program Files (x86)\\Steam");
        addDir("C:\\Program Files\\Steam");

        static QByteArray foundBuf;
        for (const QString &p : candidates) {
            foundBuf = p.toLocal8Bit();
            m_module = LoadLibraryA(foundBuf.constData());
            if (m_module) break;
            m_diag += QString("  not found: %1\n").arg(p);
        }

        if (!m_module) {
            m_diag += "SDL3.dll: NOT FOUND\n";
            m_diag += "Fix: SDL3.dll is in libs/ and should be copied next to the exe by CMake.\n";
            return;
        }
        m_diag += QString("Loaded: %1\n").arg(QString::fromLocal8Bit(foundBuf));

        m_init         = load<InitFn>        ("SDL_InitSubSystem");
        m_update       = load<UpdateFn>      ("SDL_UpdateGamepads");
        m_close        = load<CloseFn>       ("SDL_CloseGamepad");
        m_getAxis      = load<GetAxisFn>     ("SDL_GetGamepadAxis");
        m_getButton    = load<GetButtonFn>   ("SDL_GetGamepadButton");
        m_getName      = load<GetNameFn>     ("SDL_GetGamepadName");
        m_open         = load<OpenFn>        ("SDL_OpenGamepad");
        m_getJoysticks = load<GetJoysticksFn>("SDL_GetJoysticks");
        m_isGamepad    = load<IsGamepadFn>   ("SDL_IsGamepad");
        m_joyNameByID  = load<JoyNameByIDFn> ("SDL_GetJoystickNameForID");
        m_sdlFree      = load<SdlFreeFn>     ("SDL_free");

        const bool ok = m_init && m_getAxis && m_getButton && m_open &&
                        m_close && m_getJoysticks && m_isGamepad;
        m_diag += QString("Functions: %1\n").arg(ok ? "OK" : "SOME MISSING");
        if (!ok) return;

        constexpr quint32 SDL_INIT_GAMEPAD = 0x00002000u;
        const int rc = m_init(SDL_INIT_GAMEPAD);
        // SDL3 returns bool (1 byte) — check low byte only.
        const bool initOk = (rc & 0xFF) != 0;
        m_diag += QString("SDL_InitSubSystem: %1\n").arg(initOk ? "OK" : "FAILED");
        m_available = initOk;
    }

    ~SdlControllerBackend()
    {
        if (m_controller && m_close) m_close(m_controller);
    }

    void ensureController()
    {
        if (m_controller) return;

        int count = 0;
        quint32 *ids = m_getJoysticks ? m_getJoysticks(&count) : nullptr;
        m_diag += QString("Joysticks: %1\n").arg(count);

        for (int i = 0; i < count; ++i) {
            const char *name = m_joyNameByID ? m_joyNameByID(ids[i]) : nullptr;
            const bool  isGP = m_isGamepad && (m_isGamepad(ids[i]) & 0xFF) != 0;
            m_diag += QString("  [id=%1] \"%2\"  isGamepad=%3\n")
                          .arg(ids[i]).arg(name ? name : "?").arg(isGP ? "YES" : "NO");
            if (!isGP) continue;
            m_controller = m_open(ids[i]);
            if (m_controller) {
                m_diag += QString("  -> Opened: \"%1\"\n")
                              .arg(m_getName ? m_getName(m_controller) : "?");
                break;
            }
            m_diag += "  -> SDL_OpenGamepad FAILED\n";
        }

        if (ids && m_sdlFree) m_sdlFree(ids);
        if (!m_controller) m_diag += "  -> No usable gamepad found\n";
    }

    HMODULE  m_module     = nullptr;
    void    *m_controller = nullptr;
    bool     m_available  = false;
    QString  m_diag;

    InitFn         m_init         = nullptr;
    UpdateFn       m_update       = nullptr;
    CloseFn        m_close        = nullptr;
    GetAxisFn      m_getAxis      = nullptr;
    GetButtonFn    m_getButton    = nullptr;
    GetNameFn      m_getName      = nullptr;
    OpenFn         m_open         = nullptr;
    GetJoysticksFn m_getJoysticks = nullptr;
    IsGamepadFn    m_isGamepad    = nullptr;
    JoyNameByIDFn  m_joyNameByID  = nullptr;
    SdlFreeFn      m_sdlFree      = nullptr;
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
    static bool s_logged = false;
    auto &sdl2 = SdlControllerBackend::instance();
    newSdlHeld = sdl2.poll();
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
    // SDL2 section
    report += SdlControllerBackend::instance().diagnostics();

    // XInput section
    using XInputGetStateFn = DWORD (WINAPI *)(DWORD, XINPUT_STATE *);
    static HMODULE xiModule = []() -> HMODULE {
        const char *dlls[] = {"xinput1_4.dll", "xinput9_1_0.dll", "xinput1_3.dll"};
        for (const char *dll : dlls)
            if (HMODULE h = LoadLibraryA(dll)) return h;
        return nullptr;
    }();
    static XInputGetStateFn xiGetState = xiModule
        ? reinterpret_cast<XInputGetStateFn>(GetProcAddress(xiModule, "XInputGetState"))
        : nullptr;

    report += "\n=== XInput ===\n";
    if (!xiModule) {
        report += "XInput DLL: NOT FOUND\n";
    } else {
        int connected = 0;
        if (xiGetState) {
            for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
                XINPUT_STATE state{};
                if (xiGetState(i, &state) == ERROR_SUCCESS)
                    ++connected;
            }
        }
        report += QString("XInput controllers connected: %1\n").arg(connected);
        report += "(DualSense is not XInput — 0 is expected for PS5 controllers)\n";
    }
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
