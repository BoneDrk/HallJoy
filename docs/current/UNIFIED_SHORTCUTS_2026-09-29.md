# Unified command shortcuts (digital + analog) — 2026-09-29

Owner report: "Keep Alt and Tab unblocked" did not work when Alt was
gamepad-bound; a gamepad-bound letter could not be assigned as the Block Bound
Keys toggle and did not trigger it while blocked, although HallJoy clearly sees
its analog press; the Pause binding behaved worse than the Block binding.
Requirement: one designed system in which HallJoy sees both digital and analog
presses for assigning and triggering shortcuts, without conflicts, e.g. a
digital Ctrl with an analog W, working with Block Bound Keys enabled.

## Root cause

- Block toggle used a virtual-key chord, `RegisterHotKey` and the low-level
  hook only; Pause used a separate model (single HID keys, analog aware, UI
  timer capture). Two independent implementations.
- A bound key blocked by HallJoy or suppressed by keyboard firmware never
  produced the digital event the Block toggle needed; capture used
  `WM_KEYDOWN`, so such a key could not even be assigned.
- K4 HE onboard firmware (`HJO_SUPPRESS`) removes every bound key from keyboard
  output, including Tab/Alt: the host-side "Keep Alt and Tab" exception could
  not apply to keys the firmware never sent.

## Design

`input_shortcuts.h` (pure, tested) + `input_shortcuts_runtime.h` (Win32 glue).

- Shortcut = HID key (4..231) + Ctrl/Alt/Shift/Win; a lone modifier is allowed.
- Per key: digital (hook) OR analog (realtime, hysteresis 120/60 milli) = held.
  One physical press issues at most one command, from whichever signal comes first.
- Modifiers may be digital or analog. A gamepad-bound modifier (e.g. Shift as
  sprint) is ignored when not part of the chord; an unbound extra modifier
  changes the chord.
- The press that issued a command is swallowed (its digital down, repeats and
  up) before Block Bound Keys routing; other keys are untouched.
- Commands: Block toggle, Pause/Resume toggle, Pause, Resume. Applicability is
  checked at the press (Block toggle not while paused; Pause only when active,
  Resume only when paused). Not applicable = not swallowed.
- Capture (both pages): the engine records the first fresh press (digital or
  analog) with held modifiers; a lone modifier is captured on its release; Esc,
  focus loss, hiding or clicking the button again cancels. Keys held when capture
  starts must be released first. Duplicate shortcuts across active commands are
  rejected.
- Pause completion clears frozen analog samples so the Resume key works.
- `RegisterHotKey`/`WM_HOTKEY` and the Pause UI polling timer were removed.
- K4 onboard keeps reading depth while any shortcut is assigned.

Settings: `Main/BlockKeysShortcut`, `Window/PauseShortcutToggle|Pause|Resume`,
`Window/PauseSeparate`. Legacy `BlockKeysHotkey` (VK chord) and `PauseHotkeys`
(packed single keys) are migrated on load and removed on the next save.

## K4 HE onboard Alt/Tab

Host sends `HJO_KEEP_ALT_TAB` (Block on + Keep Alt/Tab on) only when the
firmware advertises capability bit 5. Firmware source r7 implements it via the
shared `hjo_suppressed()`; it is NOT built or flashed. With r6 (installed) the
flag is stripped, so Alt/Tab stay suppressed on the K4 while Block Bound Keys is
on until r7 is flashed. Other keyboards: the host exception works as before.

## Validation

- `input_shortcuts_test` (portable): dedup, mixed chords, bound modifiers,
  applicability, pause reset, seeding, capture — PASS.
- `pause_hotkeys_windows_test` (runtime glue, real message window) — PASS.
- `block_keys_policy`, hook-thread test, profile transaction test updated
  (legacy VK and legacy pause migration cases added).
- K4 host profile test: flag requested by default, stripped rule, shared
  `hjo_suppressed()` truth cases — PASS.
- `run_native_backend_checks.py --require-compiler`: all static and portable
  C++ tests PASS. `run_profile_transaction_tests.ps1` (private desktop, incl.
  Global page Pause shortcut controls and migrations): PASS, 18 recovery scenarios PASS.
- `build_release.ps1` EXIT=0; installed `build/bin/Release/x64/HallJoy.exe`
  SHA256 `5c4d4c83151c9c53f59cfaf4136a08bdfbb24c0527b84b7f5a133bb4e0173a03`;
  `--halljoy-require-full-catalog` exit 0.
- No physical keyboard or game verification by the agent.

Backup before the change: `.local/backups/src-before-unified-shortcuts-2026-09-29.tgz`.

## 2026-09-30 fix: Resume via shortcut immediately re-paused

Owner report: the Pause shortcut (Ctrl+Alt+Numpad8, where Numpad8 is bound to LB)
did not end Pause. The report noted this happened while HallJoy was focused.

Cause: with K4 firmware r8, Resume takes ~35 ms instead of ~2.5 s. While paused,
Resume fires on the digital press; analog sampling restarts while the key is
still held. The digital release can arrive while the key is still deeper than
the 120 milli threshold. The first analog samples then counted as a new press,
and PauseToggle paused again. The window-focus dependence was not reproduced or
explained.

Fix (`input_shortcuts.h`):
- A key that issued a command is latched. The analog path cannot issue another
  command from it until its analog depth is seen at or below 60 milli.
- A new digital down clears the latch, so digital-only use is never blocked by
  an unobserved analog release. `ResetAnalog` leaves the latch in place.

Also fixed: with Num Lock on, Windows wraps Shift+numpad in synthesized
E0 2A / E0 36 Shift events. The hook and the Raw Input preview now ignore them
(`IsSyntheticNumpadShift`), so Ctrl+Shift+Numpad shortcuts match on the digital
path. This was not the owner's case.

Tests: `input_shortcuts_test` includes the owner case and the synthetic Shift
stream. It fails without the latch.

## 2026-09-30 fix: shortcuts from remote access and injected input

Owner report: over remote access, shortcuts neither triggered nor could be
assigned.

Cause: the hook fed shortcuts only events without `LLKHF_INJECTED`. Remote
desktop and automation tools (AnyDesk, TeamViewer, Parsec, ...) inject their
keys, so both triggering and capture ignored them. HallJoy itself never injects
keyboard input.

Fix:
- Every keyboard event, physical or injected, now reaches `shortcuts::Digital`
  (trigger and capture). The press that issued a command is swallowed as before.
- Block Bound Keys routing, input-privilege evidence and the Ctrl+Alt+Del
  mouse rescue still use physical input only.
- `HidFromHookKey` derives the physical key from the virtual key when an
  injector sends scan code 0. Extended navigation keys are listed explicitly,
  because `MapVirtualKey` omits their E0 prefix.

Any physical keyboard already worked through the digital path; chords may mix
keyboards (e.g. Ctrl on one, Numpad8 on another). Remote input carries no
analog depth: shortcuts there are digital only.
