#include "input_shortcuts.h"
#ifdef _WIN32
// Windows hook scan-code mapping; the shortcut engine itself is portable.
#include "keyboard_scan_hid.h"
#endif
#include <cassert>
#include <iostream>
#include <vector>

using namespace halljoy::shortcuts;

namespace {
constexpr unsigned W = 26, S = 22, F8 = 65, LCtrl = 224, LShift = 225, LAlt = 226, Esc = 41;

struct Harness {
    Engine engine;
    Bindings bindings{};
    bool allow[kActionCount + 1]{false, true, true, true, true};
    bool bound[256]{};
    std::vector<Action> fired;

    bool Digital(unsigned hid, bool down) {
        return engine.Digital(hid, down, bindings,
            [&](Action a) { return allow[static_cast<unsigned>(a)]; },
            [&](unsigned h) { return bound[h]; },
            [&](Action a) { fired.push_back(a); });
    }
    void Analog(unsigned hid, unsigned milli) {
        engine.Analog(hid, milli, bindings,
            [&](Action a) { return allow[static_cast<unsigned>(a)]; },
            [&](unsigned h) { return bound[h]; },
            [&](Action a) { fired.push_back(a); });
    }
    Action Take() {
        if (fired.empty()) return Action::None;
        const Action a = fired.front();
        fired.erase(fired.begin());
        return a;
    }
};
}

int main() {
    // Packing and validation.
    assert(Valid(0) && Valid(W) && Valid(Make(W, kCtrl | kShift)) && Valid(LCtrl));
    assert(!Valid(Make(LCtrl, kShift)) && !Valid(3) && !Valid(232) && !Valid(1u << 12));
    assert(Key(Make(W, kCtrl)) == W && Mods(Make(W, kCtrl)) == kCtrl);

    {   // Analog only (digital blocked by HallJoy or firmware) with hysteresis.
        Harness h; h.bindings[0] = W;
        h.Analog(W, 119); assert(h.Take() == Action::None);
        h.Analog(W, 120); assert(h.Take() == Action::BlockToggle);
        // The digital event of the same physical press is swallowed, not re-fired.
        assert(h.Digital(W, true)); assert(h.Digital(W, true)); assert(h.Take() == Action::None);
        assert(h.Digital(W, false));
        h.Analog(W, 61); assert(h.Take() == Action::None);
        h.Analog(W, 60);
        h.Analog(W, 500); assert(h.Take() == Action::BlockToggle);
        h.Analog(W, 0);
    }
    {   // Digital only: once per press, repeats and release follow the down.
        Harness h; h.bindings[0] = F8;
        assert(h.Digital(F8, true)); assert(h.Take() == Action::BlockToggle);
        assert(h.Digital(F8, true)); assert(h.Take() == Action::None);
        assert(h.Digital(F8, false));
        assert(!h.Digital(W, true)); assert(!h.Digital(W, false));
    }
    {   // Mixed chord: digital Ctrl + analog W, and analog Ctrl + digital W.
        Harness h; h.bindings[0] = Make(W, kCtrl);
        h.Analog(W, 900); assert(h.Take() == Action::None); // no Ctrl yet
        h.Analog(W, 0);
        assert(!h.Digital(LCtrl, true));
        h.Analog(W, 900); assert(h.Take() == Action::BlockToggle);
        h.Analog(W, 0); assert(!h.Digital(LCtrl, false));
        h.Analog(LCtrl, 800);
        assert(h.Digital(W, true)); assert(h.Take() == Action::BlockToggle);
        assert(h.Digital(W, false)); h.Analog(LCtrl, 0);
    }
    {   // Unrelated modifiers: an unbound Alt changes the chord; a gamepad-bound
        // Shift (sprint) is a game control and does not.
        Harness h; h.bindings[1] = F8; h.bound[LShift] = true;
        assert(!h.Digital(LAlt, true));
        assert(!h.Digital(F8, true)); assert(h.Take() == Action::None);
        assert(!h.Digital(F8, false)); assert(!h.Digital(LAlt, false));
        h.Analog(LShift, 900);
        assert(h.Digital(F8, true)); assert(h.Take() == Action::PauseToggle);
        assert(h.Digital(F8, false)); h.Analog(LShift, 0);
    }
    {   // Not applicable now: no command and the key is not swallowed.
        Harness h; h.bindings[2] = F8; h.bindings[3] = S;
        h.allow[static_cast<unsigned>(Action::Pause)] = false;
        assert(!h.Digital(F8, true)); assert(h.Take() == Action::None);
        assert(!h.Digital(F8, false));
        assert(h.Digital(S, true)); assert(h.Take() == Action::Resume);
        assert(h.Digital(S, false));
    }
    {   // Pause stops analog readers: a frozen held sample must not hide the
        // next ordinary press used to resume.
        Harness h; h.bindings[1] = W;
        h.Analog(W, 900); assert(h.Take() == Action::PauseToggle);
        assert(h.Digital(W, true)); assert(h.Take() == Action::None);
        h.engine.ResetAnalog();
        assert(h.Digital(W, false));
        assert(h.Digital(W, true)); assert(h.Take() == Action::PauseToggle);
        assert(h.Digital(W, false));
    }
    {   // Keys held when the hook starts never trigger until released.
        Harness h; h.bindings[0] = F8;
        h.engine.SeedDigital(F8);
        assert(!h.Digital(F8, true)); assert(h.Take() == Action::None);
        assert(!h.Digital(F8, false));
        assert(h.Digital(F8, true)); assert(h.Take() == Action::BlockToggle);
        assert(h.Digital(F8, false));
    }
    {   // Capture: held key ignored, analog or digital press, chord, lone
        // modifier on release, Escape cancels; no command fires meanwhile.
        Harness h; h.bindings[0] = W;
        h.Analog(S, 900);
        h.engine.BeginCapture();
        h.Analog(S, 950); assert(!h.engine.TakeCapture());
        h.Analog(W, 900); assert(h.Take() == Action::None);
        assert(h.engine.TakeCapture() == W && !h.engine.Capturing());
        h.Analog(W, 0); h.Analog(S, 0);

        h.engine.BeginCapture();
        assert(!h.Digital(LCtrl, true));
        h.Analog(S, 900);
        assert(h.engine.TakeCapture() == Make(S, kCtrl));
        h.Analog(S, 0); assert(!h.Digital(LCtrl, false));

        h.engine.BeginCapture();
        assert(!h.Digital(LAlt, true)); assert(!h.engine.TakeCapture());
        assert(!h.Digital(LAlt, false)); assert(h.engine.TakeCapture() == LAlt);

        h.engine.BeginCapture();
        assert(!h.Digital(Esc, true)); assert(h.engine.TakeCapture() == kCaptureCancelled);
        assert(!h.Digital(Esc, false));

        h.engine.BeginCapture(); h.engine.CancelCapture();
        h.Analog(W, 900); assert(h.Take() == Action::BlockToggle);
    }
    {   // Analog sampling set: only bound keys and modifiers; everything while capturing.
        Harness h; unsigned count = 0;
        h.engine.ForEachAnalogKey(h.bindings, [&](unsigned) { ++count; }); assert(count == 0);
        h.bindings[1] = W;
        h.engine.ForEachAnalogKey(h.bindings, [&](unsigned) { ++count; }); assert(count == 9);
        count = 0; h.engine.BeginCapture();
        h.engine.ForEachAnalogKey(h.bindings, [&](unsigned) { ++count; }); assert(count == 228);
    }
    {   // Owner case Ctrl+Alt+Numpad8 toggle: Resume fires digitally while
        // paused; the engine restarts within ms, the digital release arrives
        // while the key is still deep. The first analog samples of that same
        // press must not toggle Pause back on.
        Harness h; h.bindings[1] = Make(96, kCtrl | kAlt);
        assert(!h.Digital(LCtrl, true)); assert(!h.Digital(LAlt, true));
        assert(h.Digital(96, true)); assert(h.Take() == Action::PauseToggle);
        h.engine.ResetAnalog(); // state change; analog restarts afterwards
        assert(h.Digital(96, false));
        h.Analog(96, 400); h.Analog(96, 130); assert(h.Take() == Action::None);
        h.Analog(96, 40); // released
        h.Analog(96, 400); assert(h.Take() == Action::PauseToggle); // a real new press
        h.Analog(96, 0);
        // A new digital press is never held back by an unobserved analog release.
        h.Analog(96, 400); assert(h.Take() == Action::PauseToggle);
        // Digital events of the press that issued the command stay swallowed.
        assert(h.Digital(96, true)); assert(h.Digital(96, false)); assert(h.Take() == Action::None);
        assert(h.Digital(96, true)); assert(h.Take() == Action::None); // still analog-held
        h.Analog(96, 0); assert(h.Digital(96, false));
        assert(h.Digital(96, true)); assert(h.Take() == Action::PauseToggle);
        assert(h.Digital(96, false));
    }
#ifdef _WIN32
    {   // Num Lock on: Ctrl+Shift+Numpad8 as the hook sees it while paused
        // (digital only). Windows inserts a synthesized E0 2A Shift release
        // before the numpad key; the hook must ignore it or Shift looks released.
        Harness h; h.bindings[1] = Make(96, kCtrl | kShift);
        struct Event { DWORD scan; bool ext, down; };
        const Event stream[] = {{0x1D,false,true},{0x2A,false,true},{0x2A,true,false},{0x48,false,true},
                                {0x48,false,false},{0x2A,true,true},{0x2A,false,false},{0x1D,false,false}};
        bool swallowedKeypad = false;
        for (const auto& e : stream) {
            if (IsSyntheticNumpadShift(e.scan, e.ext)) continue;
            const auto hid = HidFromKeyboardScanCode(e.scan, e.ext, 0);
            const bool swallow = h.Digital(hid, e.down);
            if (hid == 96 && e.down) swallowedKeypad = swallow;
        }
        assert(HidFromKeyboardScanCode(0x48, false, 0) == 96);
        // Remote-desktop/automation injection may carry only a virtual key.
        assert(HidFromHookKey(0, false, VK_NUMPAD8) == 96);
        assert(HidFromHookKey(0, false, VK_UP) == HidFromKeyboardScanCode(0x48, true, VK_UP));
        assert(HidFromHookKey(0, false, VK_LCONTROL) == LCtrl && HidFromHookKey(0, false, 'W') == W);
        assert(HidFromHookKey(0x11, false, 'W') == W);
        assert(swallowedKeypad && h.Take() == Action::PauseToggle && h.Take() == Action::None);
        assert(!IsSyntheticNumpadShift(0x2A, false) && !IsSyntheticNumpadShift(0x36, false) &&
               IsSyntheticNumpadShift(0x36, true) && !IsSyntheticNumpadShift(0x1D, true));
    }
#endif
    std::cout << "INPUT_SHORTCUTS=PASS synthetic numpad Shift ignored, digital/analog dedup, mixed chords, bound modifiers, applicability, pause reset, seeding, capture\n";
    return 0;
}
