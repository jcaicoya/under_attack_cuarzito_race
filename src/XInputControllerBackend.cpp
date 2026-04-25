#include "XInputControllerBackend.h"

#ifdef Q_OS_WIN

XInputControllerBackend &XInputControllerBackend::instance()
{
    static XInputControllerBackend inst;
    return inst;
}

XInputControllerBackend::XInputControllerBackend()
{
    m_diag += "=== XInput ===\n";

    const char *dlls[] = {"xinput1_4.dll", "xinput9_1_0.dll", "xinput1_3.dll"};
    const char *loadedDll = nullptr;
    for (const char *dll : dlls) {
        if (HMODULE handle = LoadLibraryA(dll)) {
            m_module = handle;
            loadedDll = dll;
            break;
        }
    }

    if (!m_module) {
        m_diag += "XInput DLL: NOT FOUND\n";
        return;
    }

    m_diag += QString("Loaded: %1\n").arg(loadedDll);
    m_getState = reinterpret_cast<XInputGetStateFn>(
        GetProcAddress(m_module, "XInputGetState"));
    if (!m_getState)
        m_diag += "XInputGetState: NOT FOUND\n";
}

QSet<Action> XInputControllerBackend::poll() const
{
    QSet<Action> actions;
    if (!m_getState)
        return actions;

    XINPUT_STATE state = {};
    for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
        if (m_getState(index, &state) != ERROR_SUCCESS)
            continue;

        const WORD buttons = state.Gamepad.wButtons;
        const SHORT lx = state.Gamepad.sThumbLX;
        const SHORT ly = state.Gamepad.sThumbLY;
        constexpr SHORT deadZone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;

        if ((buttons & XINPUT_GAMEPAD_DPAD_LEFT) || lx < -deadZone)
            actions.insert(Action::MoveLeft);
        if ((buttons & XINPUT_GAMEPAD_DPAD_RIGHT) || lx > deadZone)
            actions.insert(Action::MoveRight);
        if ((buttons & XINPUT_GAMEPAD_DPAD_UP) || ly > deadZone)
            actions.insert(Action::MoveUp);
        if ((buttons & XINPUT_GAMEPAD_DPAD_DOWN) || ly < -deadZone)
            actions.insert(Action::MoveDown);
        if ((buttons & XINPUT_GAMEPAD_A) || (buttons & XINPUT_GAMEPAD_START))
            actions.insert(Action::Confirm);
        if ((buttons & XINPUT_GAMEPAD_A) || state.Gamepad.bRightTrigger > 30)
            actions.insert(Action::Accelerate);
        if ((buttons & XINPUT_GAMEPAD_B) || (buttons & XINPUT_GAMEPAD_BACK))
            actions.insert(Action::Cancel);
        if ((buttons & XINPUT_GAMEPAD_B) || state.Gamepad.bLeftTrigger > 30)
            actions.insert(Action::Brake);

        break;
    }

    return actions;
}

QString XInputControllerBackend::diagnostics() const
{
    QString report = m_diag;
    if (!m_module || !m_getState)
        return report;

    int connected = 0;
    for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
        XINPUT_STATE state{};
        if (m_getState(i, &state) == ERROR_SUCCESS)
            ++connected;
    }

    report += QString("XInput controllers connected: %1\n").arg(connected);
    report += "(DualSense is not XInput - 0 is expected for PS5 controllers)\n";
    return report;
}

#endif // Q_OS_WIN
