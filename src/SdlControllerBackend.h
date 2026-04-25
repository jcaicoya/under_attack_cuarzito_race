#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <QSet>
#include <QString>
#include <QStringList>

#include "InputAction.h"

// Loads SDL3.dll at runtime and polls a connected gamepad.
// Singleton — use SdlControllerBackend::instance().
// Windows-only; the entire class is absent on other platforms.
class SdlControllerBackend {
public:
    static SdlControllerBackend &instance();

    // Accumulated diagnostic log (SDL load status, detected controllers).
    const QString &diagnostics() const { return m_diag; }

    // Returns the set of currently-held actions from the first connected gamepad.
    QSet<Action> poll();

private:
    using InitFn         = int         (__cdecl *)(quint32);
    using UpdateFn       = void        (__cdecl *)();
    using CloseFn        = void        (__cdecl *)(void *);
    using GetAxisFn      = qint16      (__cdecl *)(void *, int);
    using GetButtonFn    = int         (__cdecl *)(void *, int);
    using GetNameFn      = const char *(__cdecl *)(void *);
    using OpenFn         = void *      (__cdecl *)(quint32);
    using GetJoysticksFn = quint32 *   (__cdecl *)(int *);
    using IsGamepadFn    = int         (__cdecl *)(quint32);
    using JoyNameByIDFn  = const char *(__cdecl *)(quint32);
    using SdlFreeFn      = void        (__cdecl *)(void *);

    SdlControllerBackend();
    ~SdlControllerBackend();

    static QString regString(HKEY root, const char *subKey, const char *value);

    template <typename Fn>
    Fn load(const char *name) const
    {
        return reinterpret_cast<Fn>(GetProcAddress(m_module, name));
    }

    void ensureController();

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

#endif // Q_OS_WIN
