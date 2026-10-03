# Startup optimisation (2026-10-01)

Owner request: deep startup and settings optimisation, with nothing broken and
no visual artifacts. Every change below was measured on the owner's PC with
the in-process timeline (`--halljoy-perf-log`, see
[PERF_PROFILE_2026-10-01.md](PERF_PROFILE_2026-10-01.md)).
`tools/run_perf_profile.ps1` and `tools/analyze_perf_profile.py` reproduce the
measurements. Times are ms since process creation.

## Where the time went (before)

| Stage | Cost | Cause |
| --- | --- | --- |
| Process start to `wWinMain` | ~100 | Image size, see below |
| Profiles and settings | 185 | `settings.ini` parsed 3 times with one Win32 parse per key; layout catalog 110 |
| Layout catalog, built-ins | 54 | Every name compare ran Unicode NFC normalisation twice (O(n²)) |
| Layout catalog, user files | 48 | 90 files parsed serially |
| First canvas frame | 270 | The first Direct2D device (GPU driver initialisation) on the UI thread |
| Engine start | after page creation | Discovery waited for ~90 ms of UI building |
| MAD68 Dual discovery | 64 per start and resume | Opened every HID interface on the machine |
| AULA SparkPlayJoy discovery | 2 × ~20 per start and resume | The worker repeated the enumeration that pre-UAP discovery had just done |

## Changes

1. **INI read snapshot** (`bounded_ini.h`: `ini::ReadSnapshot`, `ini::LoadSectionValues`).
   - While a loader holds the `ini::ReadFile` lease, `ini::Read` for that path
     is served from sections loaded once each (`GetPrivateProfileSectionW`;
     Win32 still decodes the file).
   - It matches the per-key Win32 result exactly:
     - trimming and quotes;
     - first duplicate wins;
     - the requested key is trimmed of spaces, not tabs;
     - truncation means failure.
   - `layout_storage::Section` now uses the same parser.
   - Used by `SettingsIni_Load_Core` and `Profile_PrepareIni`.
   - Differential test `tests/ini_read_snapshot_windows_test.cpp` (native
     checks) runs synthetic edge cases in UTF-16 and ANSI, 8 buffer limits
     and nesting.
   - Run once by the agent against real data (0 mismatches):
     - the owner's AppData: 125 files, 19,727 keys;
     - the whole repository: 4,617 INI files, 515,914 keys.
   - The test also documents a Windows defect: a key made only of spaces
     crashes kernel32.
2. **Layout catalog**
   - Canonical name keys are memoised (`NamesEquivalent`, mutex-protected,
     same semantics as `FileNamePolicy_Equivalent`).
   - User preset files are parsed on up to 3 helper threads; registration,
     migrations and duplicate resolution stay sequential in directory order.
3. **Direct2D warm-up** (`keyboard_canvas::WarmUpAsync`).
   - The factory is multi-threaded. A worker creates the first device on a
     hidden message-only window and loads the DirectWrite system fonts at the
     start of `App_Run`, for real UI runs only (test modes return earlier).
   - The canvas target is then created in about 0.5 ms instead of about 250 ms.
   - Before the main window is first shown, `WaitForWarmUp(500, hwnd,
     WM_APP_ENGINE_RUNTIME_UI_OPERATION)` lets it appear once, with the
     keyboard already drawn.
   - During that wait it keeps serving sent messages and the engine's posted
     UI-bridge operation, so engine readiness does not wait for the window.
   - `wWinMain` joins the worker on every return.
4. **Engine starts before the keyboard page is built.**
   - Timing settings, the closed admission gate and Raw Input registration move
     with it, in the same order as before.
   - Provider discovery, UAP and ViGEm initialisation now overlap UI creation.
5. **MAD68 Dual discovery.**
   - A HID path that carries a different USB `vid_XXXX&pid_YYYY` is skipped
     without being opened (`usb_path_identity.h`).
   - Paths without a parseable USB identity, such as Bluetooth, are still
     opened and checked.
   - Verbose logs keep a line for each skipped path.
6. **AULA SparkPlayJoy.**
   - When pre-UAP discovery found no candidate and no known identity path, the
     worker's first pass reuses that result. Conditions: once, within 2 s, and
     with no device-change notification since.
   - Otherwise it enumerates as before.
7. **Perf buffers** are allocated only in perf mode.

## Result (3 runs each)

| Stage | Before | After |
| --- | --- | --- |
| Profiles and settings | 185 | 42–49 |
| Layout catalog (built-ins + user files) | 54 + 48 | 8 + 17 |
| Engine ready (`open_admission`, gamepad output live) | 710–760 | 353–422 |
| Window first shown | ~450, keyboard area blank until ~720 | 475–543, keyboard already drawn |
| Pause / resume / exit (with K4) | see PERF_PROFILE | 250–310 / ~140 / 200–240 |

The warm-up (~300–370 ms) now bounds the moment the window appears. Page
creation overlaps it and varies (104–182 ms) with CPU and loader contention.

Gates on the final build:
- `build_release.ps1`: PASS, including the production-linked profile tests.
- `run_native_backend_checks.py --require-compiler`: PASS, including
  `INI_READ_SNAPSHOT`.
- Private-desktop UI tests: 30/30 under artificial CPU load. The previous
  scheme failed about 10 % of runs under the same load.

## Not changed (owner decision)

The process needs about 100 ms before `wWinMain` because of the image size: a
synthetic 10.5 MB image takes 100–130 ms, 4 MB about 50 ms, an empty one about
12 ms. That is roughly 10 ms per MB on this PC, independent of imports, and
most likely the antivirus scanning the image.

The embedded ViGEmBus installer is 6.3 MB of the 10.6 MB EXE. It is used only
when the driver is missing. Moving it out would save about 60 ms on every
launch, but it changes the "one HallJoy.exe" delivery, so it is left to the
owner.

## Test-infrastructure fixes found on the way

- Private-desktop UI tests switched the test thread back to its original
  desktop.
  - Once a thread has created windows, Windows text services keep per-thread
    hooks, and `SetThreadDesktop` back then failed with `ERROR_BUSY`, with no
    windows left.
  - About 10 % of runs failed under load (overlay edit, tray lifecycle).
  - `halljoy::test_desktop::RunOnPrivateDesktop` runs each such test on a
    fresh thread that never leaves its private desktop. The desktop is closed
    after the thread exits.
- Tests now report the failing check, instead of a bare `false`.

## Backups

`.local/backups/*before-startup-opt*`, `*before-snapshot*`,
`*before-engine-order*`, `*before-prefilter*`, `*before-empty-reuse*`,
`*before-warmup*`, `*before-private-desktop*`.
