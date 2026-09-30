# Code audit 2026-09-29 — bugs and inconsistencies

Scope: static read-only review of `src/HallJoyProject/HallJoy` (core realtime path,
backend, ViGEm output IPC, pause/resume transactions, input hooks, UI, settings,
native device backends). No code was changed, nothing was built, run or
hardware-tested. Findings marked "verified" were re-read in code by the lead
reviewer; others are reviewer findings with the stated confidence.

## High

1. **AULA W669: unplug spins a core and leaves keys pressed** — verified.
   `aula_w669_backend.cpp` live loop (~l.508-523): a failed `Read()` only
   increments `g_failures`; the loop never exits on fatal I/O errors
   (`ERROR_DEVICE_NOT_CONNECTED` etc.). `g_connected` stays true, `Get()` has no
   age check, reconnect in `WorkerBody` is never reached. Fix: exit on non-timeout
   errors, `Clear()`, reconnect; add a freshness limit.
2. **SparkLink/Sayo discovery runs on the realtime thread** — verified.
   `SparkTickHotplug`/`SayoTickHotplug` are called from `Backend_Tick`; while no
   device is connected they call `SparkStart`/`SayoStart` every 2 s, which do
   SetupDi enumeration, `CreateFileW` on every HID interface, probes with 250 ms
   timeouts; stale restart joins up to 3 s. Result: periodic output hitches.
   Fix: move discovery/probe/stop-join to a background worker.

## Medium

3. **Overlay server: no Host header check (DNS rebinding)** — verified.
   `OverlayOriginAllowed` accepts requests without Origin; `GET /` issues the
   session cookie. A rebinding page can read live key state. Fix: require Host
   `127.0.0.1:<port>`/`localhost:<port>`. Also `SO_REUSEADDR` before bind
   (`overlay_server.cpp` ~l.1918) → use `SO_EXCLUSIVEADDRUSE`.
4. **Factory reset can be undone by `settings.ini.pre-bundle.bak`** — verified.
   `factory_reset.cpp` moves only 5 targets; startup recovery
   (`global_profiles.cpp` ~l.410) loads `settings.ini.pre-bundle.bak`/`.bak`
   when settings.ini is missing. `GameProfiles.ini` is also left.
5. **Profile load clamps anti-deadzone against the previous output cap** —
   verified. `settings_ini.cpp` ~l.575 sets anti-deadzone before cap;
   `Settings_SetInputAntiDeadzone` clamps to the *current* cap.
6. **Factory-reset relaunch races the instance guard** — verified.
   `main.cpp`: `App_RelaunchSelf()` runs while the local `instanceGuard` still
   holds the mutex; the new process may get `Conflicted`, fail `ShowExisting`
   and show "Another HallJoy window is already running". Fix: release the guard
   before relaunch or let the child wait for the parent PID.
7. **Lost release after I/O timeout/cancel race** — mechanism verified.
   `TimedIo` returns false even if `CancelAndDrain` reaped completed bytes
   (W669, IROK ND75, MCHOSE MIX87, MAD68 dual trial, Titan68). Delta streams
   without freshness then keep a key held. Fix: keep completed data.
8. **Curve graph drag not cancelled on capture loss** (UI reviewer, high
   confidence). `keyboard_keysettings_panel_graph.cpp` `g_drag` survives
   Alt-Tab/capture loss; later mouse moves edit and save the curve. Division by
   zero-size graph rect can store NaN.
9. **AULA WIN60HE: one implausible travel sample disables analog until a
   device-change event** (backend reviewer, medium confidence).
10. **SparkLink routing claims never released** (reviewer, high confidence):
    replug into another port / second device is handed to UAP instead.
11. **SparkLink partial layout on one row timeout** (reviewer, high confidence):
    `SparkDiscoverLayout` `break`s, remaining rows are never polled.
12. **Sayo hotplug has no service-stopped gate; probes non-audited PIDs**
    (reviewer, medium confidence).
13. **Keyboard reaches modal UI outside the main-window gate** (UI reviewer):
    factory-reset `MessageBoxW` (Y = Yes), tray `TrackPopupMenu` (Exit/Pause).
    Conflicts with the owner rule "main window is not keyboard-controlled".

## Low

- Mouse→Stick: raw mouse deltas accumulate while the feature is disabled
  (`ReadMouseStickSample` is not called then), so enabling it produces a
  full-deflection flick — verified (`backend.cpp` BuildReportFramesForPad,
  `app.cpp` WM_INPUT).
- `Backend_GetStatus()` (UI/Alumix summary) consumes the ViGEm generation edge in
  `RefreshVigemOutputStatus`, which can suppress the supervisor's resubmit of
  the newest report after an output-child restart — verified, narrow.
- ATTACK SHARK diagnostic reports access failure as protocol 21 (RongYuan) instead
  of 17; self-test asserts the wrong id — verified.
- AULA W669 `DecodeTravelInfo` rejects maxima with a zero low or high byte while
  the range check allows 32..255 — verified contradiction.
- `slice75_backend.cpp:42` FNV offset typo `...8003...` vs `...8103...` — verified.
- Ctrl+Alt+Del "disable Mouse→Stick" in the keyboard hook is effectively
  unreachable (SAS is not delivered to LL hooks) — unverified at runtime.
- Unguarded `SetEvent` on wake handles racing `Stop()` (hero84, nd75, drunkdeer,
  titan68, ace68, mad68 dual); missing exception barriers in hero84, ace68,
  titan68, azoth96 workers.
- QPC `* 1000000` overflow in `SparkNowUs`, W669/WIN60HE `NowUs` (telemetry).
- IROK ND75 / RongYuan stream: no liveness heartbeat; held keys may persist if
  the stream silently stops.
- Overlay port field accepts trailing garbage; toggle animation timers keep
  firing while hidden; debug log queue unbounded (diagnostic builds only).
- Elevated ViGEm installer runs from a user-writable `%TEMP%` directory.

## Checked and found sound

Realtime loop scheduling, output scheduler, ViGEm shared-slot protocol, pause
transaction ordering (realtime joined before neutral publish), SOCD/last-key
builder, atomic INI transactions, no production digital-depth synthesis found.

## Fixes applied 2026-09-29 (owner approved; Mouse→Stick excluded — feature disabled)

Backup before changes: `.local/backups/src-before-audit-fixes-2026-09-29.tgz`.

- W669: live loop leaves the session on device-gone errors or 3 consecutive
  read failures (published depth cleared, worker reconnects); active handle is
  unregistered by RAII on every return path; wake handle closed under
  `g_signalMutex`. Note: W669 is a transport used by Redragon K673/K617 HE, not a
  catalog model, so it is correctly absent from the Sheet.
- Completion racing cancellation is kept (not discarded) in W669, IROK ND75,
  MCHOSE MIX87 and MAD68 dual trial `TimedIo`/`Io`.
- SparkLink/Sayo hotplug moved to `native_hotplug_worker.h` background workers
  (100 ms period). Services start the worker after opening their gate and join
  it before the poller stop. `Backend_Tick` performs no discovery. Sayo gained a
  service gate and `SayoStopService`. Static audits updated.
- SparkLink: layout row read retried 3 times; `SparkNowUs` overflow-safe.
- Overlay: Host header must be `127.0.0.1:<port>`/`localhost:<port>` when present.
  `SO_REUSEADDR` kept deliberately (SO_EXCLUSIVEADDRUSE can refuse rebind while
  accepted sockets are in TIME_WAIT, breaking overlay restart).
- Factory reset also moves `settings.ini.pre-bundle.bak`, `settings.ini.bak`,
  `GameProfiles.ini` into the backup.
- `Settings_SetInputEndpoints` applies anti-deadzone/output cap as a pair
  (profile load and key-settings panel).
- Instance guard released before factory-reset relaunch.
- Curve graph drag cancelled when capture is lost; no division by a collapsed
  graph rect.
- `ModalKeyboardBlock` (WH_MSGFILTER, scoped) on the factory-reset confirmation
  and tray context menu.
- ATTACK SHARK access failure reported as protocol 17; slice75 FNV constant;
  `Backend_GetStatus` no longer consumes the ViGEm generation edge;
  HERO84 worker exception barrier and wake-handle lock; ND75/DrunkDeer wake
  handle closed under `g_signalMutex`.

Deferred (need design or hardware evidence, not changed): SparkLink routing-claim
release on replug to another port (claims are published to the UAP child),
WIN60HE one-sample plausibility shutdown, W669 `DecodeTravelInfo` byte check,
Sayo non-audited PID probing, ND75/RongYuan stream heartbeat, SparkLink >5000
route saturation, diagnostic-only backends (Titan68, ACE68, Azoth96) barriers.

### Validation of the fixes

- `python tools/run_native_backend_checks.py --require-compiler` (run from
  PowerShell): EXIT=0, all static and portable C++ tests passed. From Git Bash
  the `rongyuan_stream_protocol` test exits 127/0xC0000139 because Git's older
  `libstdc++-6.dll` precedes WinLibs on PATH; it passes when linked statically or
  run from PowerShell (environment, not code).
- `tools/build_release.ps1`: EXIT=0, candidate verified and installed to
  `build/bin/Release/x64/HallJoy.exe`, SHA256
  `0fc4145f317cc693a1487884e2748f4b6dd01147a7fdecf8782f65373708b42d`.
- `HallJoy.exe --halljoy-require-full-catalog` on the installed image: exit 0.
- Updated static audits: `aula_w669_backend_static_audit.py`,
  `sparklink_cooperative_shutdown_static_audit.py`,
  `sayo_cooperative_shutdown_static_audit.py`, `factory_reset_static_audit.py`.
- No hardware, visual or live UI test was performed by the agent. Owner check
  recommended: factory-reset confirmation/tray menu ignore keyboard, curve drag
  after Alt-Tab, overlay in OBS, SparkLink/Sayo reconnect after replug.
