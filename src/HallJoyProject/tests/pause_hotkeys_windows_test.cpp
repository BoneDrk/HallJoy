#include "input_shortcuts_runtime.h"
#include "pause_hotkeys.h"
#include <cassert>
#include <iostream>
#include <vector>

// Settings/bindings stubs: this test links only the header-only runtime.
static unsigned g_block = 0, g_pause[3]{};
static bool g_separate = false;
UINT Settings_GetBlockKeysHotkey() { return g_block; }
unsigned Settings_GetPauseShortcut(unsigned slot) { return slot < 3 ? g_pause[slot] : 0; }
bool Settings_GetPauseSeparate() { return g_separate; }
bool Bindings_IsHidBound(uint16_t hid) { return hid == 225; }

using namespace halljoy::shortcuts;
static std::vector<Action> g_fired;
static bool g_allow = true;

int main() {
    const auto hwnd = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    assert(hwnd);
    applicable.store([](Action) { return g_allow; });
    dispatch.store([](Action a) { g_fired.push_back(a); });

    // Nothing assigned: no analog reads, nothing swallowed.
    unsigned reads = 0;
    Analog([&](unsigned) { ++reads; return 0; });
    assert(reads == 0 && !NeedsAnalog());
    assert(!Digital(26, true) && !Digital(26, false) && g_fired.empty());

    // Block toggle on a gamepad-bound letter: analog press works although the
    // digital event never arrives (blocked by HallJoy or keyboard firmware).
    g_block = 26;
    assert(NeedsAnalog());
    Analog([](unsigned hid) { return hid == 26 ? 900 : 0; });
    assert(g_fired.size() == 1 && g_fired[0] == Action::BlockToggle);
    Analog([](unsigned) { return 0; });

    // Pause toggle and Block toggle on different keys in one engine.
    g_pause[0] = Make(65, kCtrl);
    assert(!Digital(224, true));
    assert(Digital(65, true));
    assert(g_fired.size() == 2 && g_fired[1] == Action::PauseToggle);
    assert(Digital(65, false) && !Digital(224, false));

    // Separate mode ignores the toggle slot.
    g_separate = true; g_pause[1] = 66; g_pause[2] = 67;
    assert(!Digital(224, true) && !Digital(65, true) && g_fired.size() == 2);
    assert(!Digital(65, false) && !Digital(224, false));
    assert(Digital(66, true) && g_fired.back() == Action::Pause);
    assert(Digital(66, false));

    // Not applicable (e.g. engine in transition): passes through, no command.
    g_allow = false;
    assert(!Digital(67, true) && !Digital(67, false));
    g_allow = true;

    // Explicit capture is reported to the owner window, analog included.
    BeginCapture(hwnd);
    assert(Capturing());
    Analog([](unsigned hid) { return hid == 20 ? 700 : 0; });
    MSG msg{};
    assert(PeekMessageW(&msg, hwnd, kCaptureMessage, kCaptureMessage, PM_REMOVE));
    assert(static_cast<unsigned>(msg.lParam) == 20 && !Capturing());
    Analog([](unsigned) { return 0; });
    BeginCapture(hwnd);
    assert(!Digital(41, true) && !Digital(41, false));
    assert(PeekMessageW(&msg, hwnd, kCaptureMessage, kCaptureMessage, PM_REMOVE));
    assert(static_cast<unsigned>(msg.lParam) == kCaptureCancelled);
    BeginCapture(hwnd); CancelCapture();
    assert(!Capturing() && !PeekMessageW(&msg, hwnd, kCaptureMessage, kCaptureMessage, PM_REMOVE));

    applicable.store(nullptr); dispatch.store(nullptr);
    DestroyWindow(hwnd);
    std::cout << "SHORTCUTS_WINDOWS=PASS analog block toggle, chords, separate pause, applicability, capture messages\n";
    return 0;
}
