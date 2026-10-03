#include "keyboard_canvas.h"

#include <dwmapi.h>
#include <algorithm>
#include <atomic>
#include <process.h>

#include "keyboard_render.h"
#include "perf_trace.h"
#include <dwrite.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dwrite.lib")

namespace halljoy::keyboard_canvas {

ID2D1Factory* Factory()
{
    static ID2D1Factory* factory = [] {
        ID2D1Factory* created = nullptr;
        D2D1_FACTORY_OPTIONS options{};
        // Multi-threaded: the startup warm-up creates the first device on a
        // worker; drawing itself stays on the UI thread.
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory), &options,
                reinterpret_cast<void**>(&created))))
            created = nullptr;
        return created;
    }();
    return factory;
}

namespace {

HANDLE g_warmUpThread = nullptr;

// The first HWND render target of a factory creates its D3D device: ~250 ms of
// driver initialisation. Do that once on a worker at process start (hidden
// message-only window, 1x1 target); the factory keeps the device, so the
// canvas target on the UI thread is then created in under a millisecond.
// DirectWrite's shared factory and system font collection are warmed the same
// way. Nothing here touches the renderer's caches.
unsigned __stdcall WarmUpThread(void*)
{
    halljoy::perf::Scope scope("ui.canvas.warmup");
    if (ID2D1Factory* factory = Factory()) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"HallJoyCanvasWarmUp";
        RegisterClassW(&wc);
        HWND window = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 1, 1, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
        if (window) {
            const auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96.0f, 96.0f);
            ID2D1HwndRenderTarget* target = nullptr;
            if (SUCCEEDED(factory->CreateHwndRenderTarget(props, D2D1::HwndRenderTargetProperties(window,
                    D2D1::SizeU(1, 1), D2D1_PRESENT_OPTIONS_IMMEDIATELY), &target))) {
                target->BeginDraw();
                target->Clear(D2D1::ColorF(0, 0, 0));
                target->EndDraw();
                target->Release();
            }
            DestroyWindow(window);
        }
    }
    IDWriteFactory* write = nullptr;
    if (SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&write)))) {
        IDWriteTextFormat* format = nullptr;
        if (SUCCEEDED(write->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"", &format))) {
            IDWriteTextLayout* layout = nullptr;
            if (SUCCEEDED(write->CreateTextLayout(L"Ag", 2, format, 64.0f, 32.0f, &layout))) {
                DWRITE_TEXT_METRICS metrics{};
                layout->GetMetrics(&metrics);
                layout->Release();
            }
            format->Release();
        }
        write->Release();
    }
    return 0;
}

constexpr wchar_t kClassName[] = L"HallJoyKeyboardCanvas";
const UINT kFrameMessage = RegisterWindowMessageW(L"HallJoy.KeyboardCanvas.Frame.v1");

std::int64_t Qpc()
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

// The compositor's vblank grid in QPC ticks: one vblank time and the refresh
// period (re-read every 250 ms; extrapolated in between).
struct VBlankGrid {
    std::int64_t vblank = 0;
    std::int64_t period = 0;
    // Start of the refresh interval containing t.
    std::int64_t SlotStart(std::int64_t t) const
    {
        std::int64_t k = (t - vblank) / period;
        if (t < vblank && (t - vblank) % period) --k;
        return vblank + k * period;
    }
};

VBlankGrid CompositorGrid()
{
    static VBlankGrid grid{};
    static std::int64_t checkedAt = 0;
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    const auto now = Qpc();
    if (!grid.period || now - checkedAt > frequency.QuadPart / 4) {
        checkedAt = now;
        DWM_TIMING_INFO timing{};
        timing.cbSize = sizeof(timing);
        if (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &timing)) && timing.qpcRefreshPeriod > 0) {
            grid.period = static_cast<std::int64_t>(timing.qpcRefreshPeriod);
            grid.vblank = static_cast<std::int64_t>(timing.qpcVBlank);
        } else {
            grid.period = frequency.QuadPart / 60;
            grid.vblank = now;
        }
    }
    return grid;
}

struct State {
    HWND hwnd = nullptr;
    PaintCallback paint = nullptr;
    void* context = nullptr;
    ID2D1HwndRenderTarget* target = nullptr;
    std::atomic<std::int64_t> lastRender{ 0 };
    std::atomic<std::int64_t> dueQpc{ 0 };   // when the owed frame may be posted
    std::atomic<bool> posted{ false };   // a frame message is queued
    std::atomic<bool> pending{ false };  // the pacing thread owes one frame
    std::atomic<bool> stop{ false };
    HANDLE kick = nullptr;
    HANDLE timer = nullptr;
    HANDLE thread = nullptr;
};

State* Get(HWND hwnd)
{
    return hwnd ? reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)) : nullptr;
}

void Post(State& st)
{
    if (!st.posted.exchange(true) && !PostMessageW(st.hwnd, kFrameMessage, 0, 0))
        st.posted.store(false);
}

// Delivers an owed frame at the start of the next refresh interval.
// Pacing is time-based on purpose: DwmFlush waits for the next composition,
// and a static screen composes nothing, so it can block for seconds while the
// canvas itself is the thing that should change (frames were starved).
unsigned __stdcall PacingThread(void* argument)
{
    auto* st = static_cast<State*>(argument);
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    while (WaitForSingleObject(st->kick, INFINITE) == WAIT_OBJECT_0 && !st->stop.load()) {
        const std::int64_t remaining = st->dueQpc.load() - Qpc();
        if (remaining > 0) {
            // Relative due time in 100 ns units (negative = relative).
            LARGE_INTEGER due{};
            due.QuadPart = -std::max<std::int64_t>(1, remaining * 10000000 / frequency.QuadPart);
            if (st->timer && SetWaitableTimer(st->timer, &due, 0, nullptr, nullptr, FALSE))
                WaitForSingleObject(st->timer, INFINITE);
            else
                Sleep(static_cast<DWORD>(std::max<std::int64_t>(1, remaining * 1000 / frequency.QuadPart)));
        }
        if (st->stop.load()) break;
        if (st->pending.exchange(false)) Post(*st);
    }
    return 0;
}

void ReleaseTarget(State& st)
{
    if (!st.target) return;
    KeyboardRender_ReleaseTargetResources(st.target);
    st.target->Release();
    st.target = nullptr;
}

void Render(State& st)
{
    RECT client{};
    GetClientRect(st.hwnd, &client);
    const UINT32 width = static_cast<UINT32>(std::max<LONG>(1, client.right));
    const UINT32 height = static_cast<UINT32>(std::max<LONG>(1, client.bottom));
    if (!st.target) {
        ID2D1Factory* factory = Factory();
        if (!factory) return;
        // 96 DPI: one DIP is one device pixel; the page already scales layout.
        // Perf A/B only: --halljoy-perf-canvas-software presents through GDI.
        static const bool perfSoftware = halljoy::perf::Enabled() && wcsstr(GetCommandLineW(), L"--halljoy-perf-canvas-software");
        const auto props = D2D1::RenderTargetProperties(perfSoftware ? D2D1_RENDER_TARGET_TYPE_SOFTWARE : D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96.0f, 96.0f);
        const auto hwndProps = D2D1::HwndRenderTargetProperties(st.hwnd, D2D1::SizeU(width, height),
            D2D1_PRESENT_OPTIONS_IMMEDIATELY);
        if (FAILED(factory->CreateHwndRenderTarget(props, hwndProps, &st.target))) {
            st.target = nullptr;
            return;
        }
    } else {
        const auto size = st.target->GetPixelSize();
        if (size.width != width || size.height != height) st.target->Resize(D2D1::SizeU(width, height));
    }
    st.lastRender.store(Qpc());
    halljoy::perf::Scope frameScope("ui.canvas.frame");
    st.target->BeginDraw();
    const bool animating = st.paint ? st.paint(st.hwnd, st.target, st.context) : false;
    const auto endDrawStart = halljoy::perf::Now();
    const HRESULT hr = st.target->EndDraw();
    halljoy::perf::Span("ui.canvas.enddraw", 0, endDrawStart);
    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseTarget(st);
        RequestFrame(st.hwnd);
        return;
    }
    if (animating) RequestFrame(st.hwnd);
}

LRESULT CALLBACK CanvasProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    State* st = Get(hwnd);
    if (msg == kFrameMessage && st) {
        st->posted.store(false);
        Render(*st);
        return 0;
    }
    switch (msg) {
    case WM_NCHITTEST:
        return HTTRANSPARENT; // input goes to the key windows below
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        if (st) Render(*st);
        return 0;
    }
    case WM_SIZE:
        if (st) RequestFrame(hwnd);
        return 0;
    case WM_DISPLAYCHANGE:
        if (st) ReleaseTarget(*st), RequestFrame(hwnd);
        return 0;
    case WM_NCDESTROY:
        if (st) {
            st->stop.store(true);
            SetEvent(st->kick);
            if (st->thread) {
                WaitForSingleObject(st->thread, 2000);
                CloseHandle(st->thread);
            }
            if (st->kick) CloseHandle(st->kick);
            if (st->timer) CloseHandle(st->timer);
            ReleaseTarget(*st);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            delete st;
        }
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool RegisterClassOnce(HINSTANCE instance)
{
    static const bool registered = [instance] {
        WNDCLASSW wc{};
        wc.lpfnWndProc = CanvasProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kClassName;
        return RegisterClassW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }();
    return registered;
}

} // namespace

void WarmUpAsync()
{
    if (g_warmUpThread) return;
    g_warmUpThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, WarmUpThread, nullptr, 0, nullptr));
}

bool WaitForWarmUp(DWORD timeoutMs, HWND serveWindow, UINT serveMessage)
{
    if (!g_warmUpThread) return true;
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    const DWORD wakeMask = QS_SENDMESSAGE | (serveWindow ? QS_POSTMESSAGE : 0);
    for (;;) {
        const ULONGLONG now = GetTickCount64();
        const DWORD remaining = now >= deadline ? 0 : static_cast<DWORD>(deadline - now);
        // Wake for the worker's exit, for messages sent from other threads and
        // for the one posted message the caller must keep serving (the engine
        // UI bridge). Everything else stays queued for the normal loop.
        const DWORD wait = MsgWaitForMultipleObjectsEx(1, &g_warmUpThread, remaining, wakeMask, 0);
        if (wait == WAIT_OBJECT_0) return true;
        if (wait != WAIT_OBJECT_0 + 1) return false; // timeout or failure: show anyway
        MSG message{};
        PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE); // dispatches sent messages
        while (serveWindow && PeekMessageW(&message, serveWindow, serveMessage, serveMessage, PM_REMOVE))
            DispatchMessageW(&message);
    }
}

void JoinWarmUp()
{
    if (!g_warmUpThread) return;
    WaitForSingleObject(g_warmUpThread, 5000);
    CloseHandle(g_warmUpThread);
    g_warmUpThread = nullptr;
}

HWND Create(HWND parent, PaintCallback paint, void* context)
{
    if (!parent || !Factory()) return nullptr;
    auto instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    if (!RegisterClassOnce(instance)) return nullptr;
    HWND hwnd = CreateWindowExW(WS_EX_NOPARENTNOTIFY, kClassName, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 1, 1, parent, nullptr, instance, nullptr);
    if (!hwnd) return nullptr;
    auto* st = new State{};
    st->hwnd = hwnd;
    st->paint = paint;
    st->context = context;
    st->kick = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    // High-resolution waitable timer (Windows 10 1803+); the default timer
    // resolution would round every wait up to ~16 ms.
    st->timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!st->timer) st->timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    if (st->kick)
        st->thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, PacingThread, st, 0, nullptr));
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
    SendToBottom(hwnd);
    return hwnd;
}

void Destroy(HWND canvas)
{
    if (canvas && IsWindow(canvas)) DestroyWindow(canvas);
}

void RequestFrame(HWND canvas)
{
    State* st = Get(canvas);
    if (!st) return;
    if (st->posted.load() || st->pending.load()) return; // coalesced into the owed frame
    // One frame per refresh interval, locked to the compositor's vblank grid:
    // render at once when this interval has no frame yet, otherwise just after
    // the next vblank. Scheduling "previous frame + period" instead let every
    // frame start slightly late, so the frame rate settled below the refresh
    // rate and some refreshes showed no new frame (visible as stutter).
    const VBlankGrid grid = CompositorGrid();
    const std::int64_t now = Qpc();
    const std::int64_t last = st->lastRender.load();
    const std::int64_t slot = grid.SlotStart(now);
    if (!st->thread || !last || grid.SlotStart(last) < slot) {
        halljoy::perf::Mark("canvas.request.immediate");
        Post(*st);
        return;
    }
    halljoy::perf::Mark("canvas.request.paced");
    st->dueQpc.store(slot + grid.period + grid.period / 16);
    st->pending.store(true);
    SetEvent(st->kick);
}

void Place(HWND canvas, const RECT& bounds)
{
    if (!canvas) return;
    SetWindowPos(canvas, nullptr, bounds.left, bounds.top, std::max<LONG>(1, bounds.right - bounds.left),
        std::max<LONG>(1, bounds.bottom - bounds.top), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    RequestFrame(canvas);
}

void SendToBottom(HWND canvas)
{
    if (canvas) SetWindowPos(canvas, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void KeepAbove(HWND canvas, HWND inputWindow)
{
    if (canvas && inputWindow)
        SetWindowPos(inputWindow, canvas, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

} // namespace halljoy::keyboard_canvas
