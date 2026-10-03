#pragma once
// Private-desktop UI tests switch the calling thread between desktops.
// SetThreadDesktop fails with ERROR_BUSY while the thread still owns windows
// on its current desktop. After a test destroys its windows, Windows tears
// down the thread's default IME and text-services windows asynchronously;
// under load that can lag behind DestroyWindow. Pump the thread's messages
// and retry for a bounded time; any other error is returned at once.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>

namespace halljoy::test_desktop {

inline bool SwitchThreadDesktop(HDESK target, DWORD timeoutMs = 3000) noexcept
{
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    for (;;) {
        if (SetThreadDesktop(target)) return true;
        const DWORD error = GetLastError();
        if (error != ERROR_BUSY || GetTickCount64() >= deadline) {
            SetLastError(error);
            return false;
        }
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(10);
    }
}

// Diagnostic for a failed switch: class names of the windows this thread still
// owns on its current desktop (what keeps SetThreadDesktop busy).
inline std::string ThreadWindowsDescription()
{
    std::string text;
    EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM context) -> BOOL {
        auto& out = *reinterpret_cast<std::string*>(context);
        char name[64]{};
        GetClassNameA(window, name, sizeof(name));
        if (!out.empty()) out += ',';
        out += name;
        out += IsWindowVisible(window) ? "(visible)" : "(hidden)";
        return out.size() < 400;
    }, reinterpret_cast<LPARAM>(&text));
    return text.empty() ? std::string("none") : text;
}

// Runs a UI test on a fresh thread attached to a new private desktop. The
// thread never switches back: once a thread created windows, Windows text
// services keep per-thread hooks on that desktop for an unbounded time, so a
// switch back can fail with ERROR_BUSY (seen under load). The thread simply
// ends there; the desktop is closed after it has exited. Exceptions thrown by
// the test are rethrown on the calling thread.
template <class Test>
bool RunOnPrivateDesktop(const wchar_t* prefix, Test&& test)
{
    bool result = false;
    std::exception_ptr failure;
    HDESK desktop = nullptr;
    std::thread worker([&] {
        try {
            const std::wstring name = std::wstring(prefix) + L"-" + std::to_wstring(GetCurrentProcessId()) +
                L"-" + std::to_wstring(GetCurrentThreadId());
            desktop = CreateDesktopW(name.c_str(), nullptr, nullptr, 0, GENERIC_ALL, nullptr);
            if (!desktop) throw std::runtime_error("CreateDesktop failed " + std::to_string(GetLastError()));
            if (!SwitchThreadDesktop(desktop))
                throw std::runtime_error("SetThreadDesktop(private) failed " + std::to_string(GetLastError()));
            result = test();
        } catch (...) {
            failure = std::current_exception();
        }
    });
    worker.join();
    if (desktop) CloseDesktop(desktop);
    if (failure) std::rethrow_exception(failure);
    return result;
}

} // namespace halljoy::test_desktop
