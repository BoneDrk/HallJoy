# Performance profile on the owner's PC, and fixes (2026-10-01)

Real-hardware measurements of the exact release build, from inside the process.
Visual review is the owner's; the agent measured timings and ran automated checks.

## Tooling (local agent research, not a tester workflow)

- `perf_trace.h/.cpp`: opt-in timeline. Off unless HallJoy starts with
  `--halljoy-perf-log=<file>` (one relaxed atomic load per mark otherwise).
  - Records static names, an index and QPC times.
  - Also records per-thread CPU every 500 ms; the thread list is refreshed
    every 5 s, because a system-wide Toolhelp snapshot is itself expensive.
  - Writes the file once, at process exit.
  - In this mode only, the main window also accepts
    `WM_APP_ENGINE_RUNTIME_TOGGLE` and `WM_SYSCOMMAND` from an unelevated
    process. HallJoy runs elevated on the owner's PC through a
    `RUNASADMIN` compatibility flag.
- Marks:
  - startup stages: main, profiles/settings, window, page sub-stages;
  - every engine-owner operation (`op.*`);
  - every native provider's prepare, start and stop, with its id;
  - UAP init and ViGEm start;
  - shutdown stages;
  - canvas frames and UI ticks;
  - K4 onboard stop path and each control exchange.
- `tools/run_perf_profile.ps1 -OutDir <dir>` drives one run:
  1. start;
  2. 10 s idle with the window shown;
  3. 10 s idle minimized;
  4. N pause/resume cycles;
  5. graceful close.
- `tools/analyze_perf_profile.py <dir>` summarizes the run:
  - the timeline;
  - slow operations;
  - per-phase CPU per thread, named from the linker map;
  - UI work.
- `.local/k4_park_cycle_probe.cpp` is a hardware probe of the K4 open/park,
  reopen and release cycle. Its log is
  `.local/k4-park-cycle-probe-2026-10-01.log`.

## Findings (before fixes)

| Area | Measured | Note |
| --- | --- | --- |
| Pause (2nd and later), K4 HE onboard connected | 1.5–3.8 s | K4 stop: full STOP + USB re-enumeration wait |
| Exit | 3.5–3.8 s | same K4 path; the Reconnect deadline expired |
| Resume after the 1st pause | ~95 ms engine, then ~2.5 s K4 re-open in the worker | the K4 needed re-enumeration every time |
| Startup, to window shown | ~435 ms | |
| Startup, to engine ready | ~710 ms | |
| Idle CPU (shown, minimized, paused) | ~1–2.5 % of one core | not a priority |
| Canvas frame | mean 2.8 ms, max 18 ms | ~31 µs per key |

## Fixed in this task

1. **K4 park was undone on every pause.**
   - `KeychronOnboard_ReleaseParked()` ran inside `Backend_Shutdown()`. Pause
     also runs that function (`op.release_backend_leases`).
   - So every pause immediately STOPped the just-parked session and set
     `parkOnStop=false` permanently.
   - From the second pause on, the K4 did a full close (1.5–3.8 s), and every
     resume had to re-enumerate.
   - Now:
     - release runs only at process exit, in `AppShutdownNoThrow` after the
       engine owner stopped;
     - `ReleaseParked` no longer clears `parkOnStop`;
     - exit parks and then releases, instead of a full close plus
       re-enumeration wait.
2. **K4 stop race at exit.**
   - Closing admission makes the worker park first. The stop can cancel that
     park's retry after the firmware already parked, and the cancelled
     fallback closes the handle.
   - The stop path now:
     1. queries the firmware;
     2. if needed, reopens the handle and skips up to two stale replies left
        by the cancelled request;
     3. if the session is already PARKED, records it instead of a
        STOP + re-enumeration wait that could only time out.
3. **Flaky `support_log_windows` build gate.** These were real writer bugs,
   not timing noise:
   - `MoveFileEx` cannot replace a log that any reader has open, even with
     `FILE_SHARE_DELETE` (Windows 10: `ERROR_ACCESS_DENIED`).
     - The writer fell into its 5 s failure backoff, while the test polls the
       file (and in production Open log, an editor or antivirus may hold it).
     - The rename is now retried for up to ~1 s; the 5 s backoff remains for
       unusable destinations.
   - Research records could stay unwritten.
     - The producer sets `researchPending` after releasing the queue. A record
       taken in tick N carried its flag into an empty tick N+1, so it was not
       written until another snapshot.
     - The batch content now decides.
   - The test's temp directory was a deleted temp file name, which can race
     between concurrent test processes. It is now created atomically.
   - New regression: a busy reader holds the log 20 of every 30 ms while a
     snapshot is requested; the snapshot must complete without the 5 s backoff.
   - The locked-mirror expectation now allows the ~1 s retry before the error
     is reported.

## After fixes (3 runs, 2 cycles each)

| Operation | Time |
| --- | --- |
| Pause | 170–290 ms (K4 park 5–12 ms) |
| Resume | ~155 ms; the K4 reopens from PARKED without re-enumeration |
| Exit | 170–250 ms engine stop + ~15 ms K4 release |

Writer stress: 48/48 parallel runs of both writer test variants passed.
For comparison, the old writer failed the new regression 3 times out of 6.

Gates:
- `check_support_diagnostics.py`: PASS.
- `run_native_backend_checks.py --require-compiler`: PASS.
- `build_release.ps1`: PASS.

## Remaining potential (not changed yet), largest first

1. **Startup ordering (~300 ms).**
   - The engine starts only after the whole UI page is built (406 ms).
   - Its `op.restore_ui_input` then waits ~120–150 ms for the busy UI
     thread; the actual work is 0.03 ms.
   - Starting the engine in parallel with UI creation would bring "engine
     ready" from ~710 ms to roughly ~430 ms.
2. **Settings apply (~117 ms on the UI thread before the window exists).**
   - `SettingsIni_Load`: parse 17 ms, `apply()` 117 ms.
   - The `apply()` content is not yet broken down.
3. **Sequential provider stop (pause ~200–290 ms).**
   - Only two providers cost time: AULA Mini60 (~70–140 ms) and Attack Shark
     Pro (~100–130 ms).
   - Signalling all providers first and then joining all would cut pause to
     the slowest one.
4. **Resume/startup probes.**
   - `mad68-dual-trial` prepare: 62 ms on every resume, with no such device
     connected.
   - SparkPlayJoy: prepare 21 ms and start 22 ms.
   - UAP init: 28–57 ms.
5. **Before `wWinMain`: ~100 ms.** Loader or static initialization is
   suspected and not yet verified.
6. **Canvas frame: 2.8 ms.** Per-key clip and layer push, plus
   `GetWindowText` per key per frame. Matters only at high refresh rates with
   every key animating. Done later the same day: 1.6 ms at 200 fps, see
   [KEYBOARD_D2D_CANVAS_2026-10-01.md](KEYBOARD_D2D_CANVAS_2026-10-01.md).

Idle CPU was not a target: ~1–2.5 % of one core in every state.

## Backups

`.local/backups/*before-perf-2026-10-01*`, `*before-k4-park-fix-2026-10-01`,
`support_log.cpp.before-replace-retry-2026-10-01`,
`support_log_windows_test.cpp.before-2026-10-01`.
