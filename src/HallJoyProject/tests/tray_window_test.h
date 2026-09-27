#pragma once
#include "../HallJoy/tray_window.h"
#include "../HallJoy/ui_activity.h"
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
    std::thread thread([&] {
        HDESK previous = GetThreadDesktop(GetCurrentThreadId());
        wchar_t name[80]{};
        swprintf_s(name, L"HallJoyTrayTest.%lu.%lu", GetCurrentProcessId(), GetCurrentThreadId());
        HDESK desktop = CreateDesktopW(name, nullptr, nullptr, 0, GENERIC_ALL, nullptr);
        if (!desktop) return;
        if (!SetThreadDesktop(desktop)) { CloseDesktop(desktop); return; }
        WNDCLASSW cls{}; cls.lpfnWndProc = FixtureProc;
        cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = L"WootingVigemGui";
        const ATOM registered = RegisterClassW(&cls);
        HWND window = registered ? CreateWindowExW(0, cls.lpszClassName, L"Tray lifecycle test", WS_OVERLAPPEDWINDOW,
            0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr) : nullptr;
        if (window) {
            Window tray(Notify); tray.Attach(window, LoadIconW(nullptr, IDI_APPLICATION));
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&tray));
            failAdd = false; adds = deletes = 0;
            ShowWindow(window, SW_SHOWNORMAL);
            ok = tray.Hide() && tray.Hidden() && !IsWindowVisible(window) && adds == 1;
            ok &= tray.Hide() && adds == 1; // Idempotent close while already hidden.
            tray.ShellRestarted();
            ok &= tray.Hidden() && !IsWindowVisible(window) && adds == 2;
            ok &= ShowExisting(); // Same restore handshake used by a second EXE invocation.
            ok &= !tray.Hidden() && IsWindowVisible(window) && !IsIconic(window) && deletes == 1;
            failAdd = true;
            ok &= !tray.Hide() && IsWindowVisible(window) && !tray.Hidden();
            failAdd = false;
            ShowWindow(window, SW_SHOWMAXIMIZED);
            ok &= tray.Hide(); tray.Restore();
            ok &= IsZoomed(window) != FALSE;
            ok &= tray.Hide(); failAdd = true; tray.ShellRestarted();
            ok &= !tray.Hidden() && IsWindowVisible(window) && IsZoomed(window);
            ShowWindow(window, SW_MINIMIZE); tray.Restore();
            ok &= IsWindowVisible(window) && !IsIconic(window);
            failAdd = false;
            ok &= tray.Hide();
            const HICON activeIcon = lastIcon.hIcon;
            const int beforeModify = modifies;
            tray.SetPaused(true);
            ok &= lastIcon.hIcon && lastIcon.hIcon != activeIcon &&
                wcscmp(lastIcon.szTip, L"HallJoy - Paused") == 0 && tray.Hidden();
            const HICON pauseIcon = lastIcon.hIcon;
            tray.SetPaused(true);
            ok &= modifies == beforeModify + 1;
            tray.ShellRestarted();
            ok &= lastIcon.hIcon == pauseIcon && wcscmp(lastIcon.szTip, L"HallJoy - Paused") == 0;
            tray.SetPaused(false);
            ok &= lastIcon.hIcon == activeIcon && wcscmp(lastIcon.szTip, L"HallJoy") == 0;
            const int before = deletes;
            tray.Remove(); tray.Remove();
            ok &= deletes == before + 1;
            for (const auto& state : { MenuState{ L"Pause HallJoy", true, false, false },
                MenuState{ L"Resume HallJoy", true, true, true },
                MenuState{ L"Restart required", false, false, true } }) {
                HMENU menu = CreateMenu(state);
                ok &= menu != nullptr;
                if (menu) {
                    const UINT engine = static_cast<UINT>(state.paused ? Action::Resume : Action::Pause);
                    const UINT overlay = static_cast<UINT>(state.overlayRunning ? Action::StopOverlay : Action::StartOverlay);
                    wchar_t text[80]{};
                    GetMenuStringW(menu, engine, text, 80, MF_BYCOMMAND);
                    ok &= wcscmp(text, state.engineText) == 0;
                    ok &= ((GetMenuState(menu, engine, MF_BYCOMMAND) & MF_GRAYED) == 0) == state.engineEnabled;
                    ok &= GetMenuState(menu, overlay, MF_BYCOMMAND) != static_cast<UINT>(-1);
                    ok &= GetMenuItemCount(menu) == 6;
                    DestroyMenu(menu);
                }
            }
            DestroyWindow(window);
        }
        if (registered) UnregisterClassW(cls.lpszClassName, cls.hInstance);
        ok &= SetThreadDesktop(previous) != FALSE;
        CloseDesktop(desktop);
    });
    thread.join();
    return ok;
}
}
