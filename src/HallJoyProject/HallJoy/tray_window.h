#pragma once
#include <windows.h>
#include <shellapi.h>
#include <cstdint>

namespace halljoy::tray {
inline UINT ShowMessage() { static const UINT message = RegisterWindowMessageW(L"HallJoy.ShowMainWindow.v1"); return message; }
inline UINT ExitMessage() { static const UINT message = RegisterWindowMessageW(L"HallJoy.ExitForBuild.v1"); return message; }
inline UINT TaskbarMessage() { static const UINT message = RegisterWindowMessageW(L"TaskbarCreated"); return message; }
inline constexpr UINT Callback = WM_APP + 470;
inline constexpr LRESULT ShowAcknowledged = 0x484A;
// Opaque amber pause badge stays legible on light and dark taskbars.
inline HICON CreatePausedIcon(HICON source) {
    const int size = GetSystemMetrics(SM_CXSMICON);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size; info.bmiHeader.biHeight = -size;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP color = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    HDC dc = CreateCompatibleDC(nullptr);
    HICON result = nullptr;
    if (color && mask && dc && bits) {
        auto old = SelectObject(dc, mask);
        PatBlt(dc, 0, 0, size, size, BLACKNESS);
        SelectObject(dc, color);
        ZeroMemory(bits, size * size * 4);
        DrawIconEx(dc, 0, 0, source, size, size, 0, nullptr, DI_NORMAL);
        GdiFlush();
        auto* pixels = static_cast<std::uint32_t*>(bits);
        const int badge = (size * 3) / 4;
        const int left = size - badge, top = size - badge;
        for (int y = 0; y < badge; ++y) for (int x = 0; x < badge; ++x) {
            const int dx = 2*x + 1 - badge, dy = 2*y + 1 - badge;
            if (dx*dx + dy*dy > badge*badge) continue;
            const bool bar = y >= badge/4 && y < badge - badge/4 &&
                ((x >= badge/4 && x < badge*5/12) || (x >= badge*7/12 && x < badge*3/4));
            pixels[(top+y)*size + left+x] = bar ? 0xFF202020u : 0xFFFFC447u;
        }
        SelectObject(dc, old);
        ICONINFO iconInfo{}; iconInfo.fIcon = TRUE;
        iconInfo.hbmColor = color; iconInfo.hbmMask = mask;
        result = CreateIconIndirect(&iconInfo);
    }
    if (dc) DeleteDC(dc);
    if (mask) DeleteObject(mask);
    if (color) DeleteObject(color);
    return result;
}
enum class Action : UINT { None, Open, Exit, Pause, Resume, StartOverlay, StopOverlay };
struct MenuState {
    const wchar_t* engineText;
    bool engineEnabled;
    bool paused;
    bool overlayRunning;
};
inline HMENU CreateMenu(const MenuState& state) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return nullptr;
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(Action::Open), L"Open HallJoy");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (state.engineEnabled ? MF_ENABLED : MF_GRAYED),
        static_cast<UINT>(state.paused ? Action::Resume : Action::Pause), state.engineText);
    AppendMenuW(menu, MF_STRING,
        static_cast<UINT>(state.overlayRunning ? Action::StopOverlay : Action::StartOverlay),
        state.overlayRunning ? L"Stop Input Overlay" : L"Start Input Overlay");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(Action::Exit), L"Exit");
    SetMenuDefaultItem(menu, static_cast<UINT>(Action::Open), FALSE);
    return menu;
}

// UI-thread only. Shell failure must never strand an invisible application.
class Window {
public:
    using Notify = BOOL (WINAPI*)(DWORD, PNOTIFYICONDATAW);
    explicit Window(Notify notify = Shell_NotifyIconW) : notify_(notify) {}
    ~Window() { Remove(); if (pausedIcon_) DestroyIcon(pausedIcon_); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    void Attach(HWND window, HICON icon) { window_ = window; icon_ = icon; }
    void SetPaused(bool paused) {
        if (paused_ == paused) return;
        paused_ = paused;
        if (paused && !pausedIcon_) pausedIcon_ = CreatePausedIcon(icon_);
        if (installed_) {
            auto data = Data();
            if (!notify_(NIM_MODIFY, &data)) {
                Remove();
                if (hidden_ && !Add()) Restore();
            }
        }
    }
    bool Hidden() const { return hidden_; }
    bool Hide() {
        if (hidden_) return true;
        if (!Add()) return false;
        WINDOWPLACEMENT placement{ sizeof(placement) };
        GetWindowPlacement(window_, &placement);
        maximized_ = IsZoomed(window_) ||
            (IsIconic(window_) && (placement.flags & WPF_RESTORETOMAXIMIZED));
        hidden_ = true;
        ShowWindow(window_, SW_HIDE);
        return true;
    }
    void Restore() {
        const bool wasHidden = hidden_;
        hidden_ = false;
        ShowWindow(window_, wasHidden ? (maximized_ ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL) :
            (IsIconic(window_) ? SW_RESTORE : SW_SHOW));
        Remove();
        SetForegroundWindow(window_);
    }
    void ShellRestarted() {
        installed_ = false;
        if (hidden_ && !Add()) Restore();
    }
    void Remove() {
        if (installed_) { auto data = Data(); notify_(NIM_DELETE, &data); }
        installed_ = false;
    }
    // Explicit Exit bypasses the close-to-tray preference in the main window.
    Action ContextMenu(const MenuState& state) {
        HMENU menu = CreateMenu(state);
        if (!menu) { Restore(); return Action::None; }
        POINT point{}; GetCursorPos(&point);
        SetForegroundWindow(window_);
        const UINT action = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
            point.x, point.y, 0, window_, nullptr);
        DestroyMenu(menu);
        PostMessageW(window_, WM_NULL, 0, 0);
        if (action == static_cast<UINT>(Action::Open)) Restore();
        return static_cast<Action>(action);
    }
private:
    NOTIFYICONDATAW Data() const {
        NOTIFYICONDATAW data{}; data.cbSize = sizeof(data);
        data.hWnd = window_; data.uID = 1;
        data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        data.uCallbackMessage = Callback; data.hIcon = paused_ && pausedIcon_ ? pausedIcon_ : icon_;
        lstrcpynW(data.szTip, paused_ ? L"HallJoy - Paused" : L"HallJoy", ARRAYSIZE(data.szTip));
        return data;
    }
    bool Add() {
        auto data = Data();
        installed_ = notify_(NIM_ADD, &data) != FALSE;
        // Keep legacy callbacks intentionally: coordinates are queried on demand,
        // and WM_LBUTTONUP/WM_RBUTTONUP work without v4 packing differences.
        return installed_;
    }
    HWND window_ = nullptr;
    HICON icon_ = nullptr;
    HICON pausedIcon_ = nullptr;
    Notify notify_;
    bool hidden_ = false, installed_ = false, maximized_ = false;
    bool paused_ = false;
};

inline bool ShowExisting() {
    HWND window = FindWindowW(L"WootingVigemGui", nullptr);
    if (!window) return false;
    DWORD process = 0; GetWindowThreadProcessId(window, &process);
    AllowSetForegroundWindow(process);
    DWORD_PTR result = 0;
    return SendMessageTimeoutW(window, ShowMessage(), 0, 0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 2000, &result) && result == ShowAcknowledged;
}
}
