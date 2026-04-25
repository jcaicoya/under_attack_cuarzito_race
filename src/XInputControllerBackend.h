#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <Xinput.h>

#include <QSet>
#include <QString>

#include "InputManager.h"

// Loads XInput at runtime and polls the first connected XInput controller.
// Windows-only; the entire class is absent on other platforms.
class XInputControllerBackend {
public:
    static XInputControllerBackend &instance();

    QString diagnostics() const;
    QSet<Action> poll() const;

private:
    using XInputGetStateFn = DWORD (WINAPI *)(DWORD, XINPUT_STATE *);

    XInputControllerBackend();

    HMODULE           m_module   = nullptr;
    XInputGetStateFn  m_getState = nullptr;
    QString           m_diag;
};

#endif // Q_OS_WIN
