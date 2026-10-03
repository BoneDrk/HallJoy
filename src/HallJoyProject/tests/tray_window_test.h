#pragma once
#include "../HallJoy/test_thread_desktop.h"
#include "../HallJoy/tray_window.h"
#include "../HallJoy/ui_activity.h"
#include <stdexcept>
#include <string>
#include <thread>

namespace halljoy::tray::test {
inline bool failAdd = false;
inline int adds = 0, deletes = 0;
inline int modifies = 0;
inline NOTIFYICONDATAW lastIcon{};
inline LRESULT CALLBACK FixtureProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == ShowMessage()) {
        auto* tray = reinterpret_cast<Window*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (tray) { tray->Restore(); return ShowAcknowledged; }
    }
    return DefWindowProcW(window, message, w, l);
}
inline BOOL WINAPI Notify(DWORD action, PNOTIFYICONDATAW data) {
    if (data->uID != 1 || !data->hWnd) return FALSE;
    lastIcon = *data;
    if (action == NIM_MODIFY) ++modifies;
    if (action == NIM_ADD) { ++adds; return !failAdd; }
    if (action == NIM_DELETE) ++deletes;
    return TRUE;
}
inline bool Run() {
    bool ok = false;
    int failedLine = 0; // first failing check, reported instead of a bare false
    const auto check = [&](bool value, int line) { if (!value && !failedLine) failedLine = line; return value; };
    // Fresh thread on a private desktop that it never leaves (see RunOnPrivateDesktop).
    halljoy::test_desktop::RunOnPrivateDesktop(L"HallJoyTrayTest", [&]() -> bool {
        WNDCLASSW cls{}; cls.lpfnWndProc = FixtureProc;
        cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = L"WootingVigemGui";
        const ATOM registered = RegisterClassW(&cls);
        HWND window = registered ? CreateWindowExW(0, cls.lpszClassName, L"Tray lifecycle test", WS_OVERLAPPEDWINDOW,
            0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr) : nullptr;
        check(window != nullptr, __LINE__);
        if (window) {
            Window tray(Notify); tray.Attach(window, LoadIconW(nullptr, IDI_APPLICATION));
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&tray));
            failAdd = false; adds = deletes = 0;
            ShowWindow(window, SW_SHOWNORMAL);
            ok = check(tray.Hide() && tray.Hidden() && !IsWindowVisible(window) && adds == 1, __LINE__);
            ok &= check(tray.Hide() && adds == 1, __LINE__); // Idempotent close while already hidden.
            tray.ShellRestarted();
            ok &= check(tray.Hidden() && !IsWindowVisible(window) && adds == 2, __LINE__);
            ok &= check(ShowExisting(), __LINE__); // Same restore handshake used by a second EXE invocation.
            ok &= check(!tray.Hidden() && IsWindowVisible(window) && !IsIconic(window) && deletes == 1, __LINE__);
            failAdd = true;
            ok &= check(!tray.Hide() && IsWindowVisible(window) && !tray.Hidden(), __LINE__);
            failAdd = false;
            ShowWindow(window, SW_SHOWMAXIMIZED);
            ok &= check(tray.Hide(), __LINE__); tray.Restore();
            ok &= check(IsZoomed(window) != FALSE, __LINE__);
            ok &= check(tray.Hide(), __LINE__); failAdd = true; tray.ShellRestarted();
            ok &= check(!tray.Hidden() && IsWindowVisible(window) && IsZoomed(window), __LINE__);
            ShowWindow(window, SW_MINIMIZE); tray.Restore();
            ok &= check(IsWindowVisible(window) && !IsIconic(window), __LINE__);
            failAdd = false;
            ok &= check(tray.Hide(), __LINE__);
            const HICON activeIcon = lastIcon.hIcon;
            const int beforeModify = modifies;
            tray.SetPaused(true);
            ok &= check(lastIcon.hIcon && lastIcon.hIcon != activeIcon &&
                wcscmp(lastIcon.szTip, L"HallJoy - Paused") == 0 && tray.Hidden(), __LINE__);
            const HICON pauseIcon = lastIcon.hIcon;
            tray.SetPaused(true);
            ok &= check(modifies == beforeModify + 1, __LINE__);
            tray.ShellRestarted();
            ok &= check(lastIcon.hIcon == pauseIcon && wcscmp(lastIcon.szTip, L"HallJoy - Paused") == 0, __LINE__);
            tray.SetPaused(false);
            ok &= check(lastIcon.hIcon == activeIcon && wcscmp(lastIcon.szTip, L"HallJoy") == 0, __LINE__);
            const int before = deletes;
            tray.Remove(); tray.Remove();
            ok &= check(deletes == before + 1, __LINE__);
            for (const auto& state : { MenuState{ L"Pause HallJoy", true, false, false },
                MenuState{ L"Resume HallJoy", true, true, true },
                MenuState{ L"Restart required", false, false, true } }) {
                HMENU menu = CreateMenu(state);
                ok &= check(menu != nullptr, __LINE__);
                if (menu) {
                    const UINT engine = static_cast<UINT>(state.paused ? Action::Resume : Action::Pause);
                    const UINT overlay = static_cast<UINT>(state.overlayRunning ? Action::StopOverlay : Action::StartOverlay);
                    wchar_t text[80]{};
                    GetMenuStringW(menu, engine, text, 80, MF_BYCOMMAND);
                    ok &= check(wcscmp(text, state.engineText) == 0, __LINE__);
                    ok &= check(((GetMenuState(menu, engine, MF_BYCOMMAND) & MF_GRAYED) == 0) == state.engineEnabled, __LINE__);
                    ok &= check(GetMenuState(menu, overlay, MF_BYCOMMAND) != static_cast<UINT>(-1), __LINE__);
                    ok &= check(GetMenuItemCount(menu) == 6, __LINE__);
                    DestroyMenu(menu);
                }
            }
            DestroyWindow(window);
        }
        if (registered) UnregisterClassW(cls.lpszClassName, cls.hInstance);
        return ok;
    });
    if (!ok) throw std::runtime_error("tray lifecycle failed at tray_window_test.h line " + std::to_string(failedLine));
    return ok;
}
}
