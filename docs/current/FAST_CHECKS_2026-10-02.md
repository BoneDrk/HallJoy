# Faster build gates without dropping checks (2026-10-02)

Owner complaint: about 5 minutes of work were followed by about 15 minutes of
compilation and checks. Goal: the same checks with the same strictness, in less
time.

## Measured (before)

| Step | Time |
|---|---|
| `check_support_diagnostics.py` (7 g++ test builds, one after another) | 147 s |
| `research_reference_checks.py` (13 replays, one after another; `research_layout_checks.py` alone 57 s) | 65 s |
| other static audits (116 scripts) | ~10 s |
| MSBuild Release, 1 file changed | 7 s; a header change rebuilt 115 files on one core (no `/MP`) |
| Simulator build + profile/layout tests | 8 s + 25 s |
| `run_native_backend_checks.py --require-compiler` (about 180 test builds, one after another) | several minutes |

## Changes

- `run_native_backend_checks.py`: `compile_and_run_many` compiles all test
  binaries in parallel (one per CPU), then RUNS every test in order, as before.
  Same compiler, flags, sources and tests. `check_support_diagnostics.py` uses
  it too.
- Test-binary cache (`build/obj/portable-tests/cache/`): a binary is reused
  only when all of the following are identical:
  - the compiler executable (path, size, timestamp);
  - the full command line;
  - the bytes of every file the compiler read (its `-MD` list, system headers
    included).

  If any input changes during compilation, the binary is not cached. Every
  test still runs every time. `HALLJOY_NO_TEST_CACHE=1` forces fresh compiles.
- `research_reference_checks.py`: the 13 replays run concurrently. Output is
  printed in the fixed order and any failure fails the gate.
- `research_layout_checks.py`: each per-brand check runs in full in its own
  process, and these run in parallel (57 s → 16 s). Under `--record` tracing
  (`HALLJOY_RESEARCH_TRACE=1`) it stays in-process so every layout input is
  still pinned. The re-recorded manifest covers the same 842 files.
- `HallJoy.vcxproj`: `MultiProcessorCompilation` (cl `/MP`) for Debug and
  Release. This changes compilation parallelism only, not compiler options or
  output.
- Three long tests run in the background next to the ordered sequential run:
  `support_log_windows`, `input_path_log_windows` and
  `process_generation_supervisor`. They wait on real timers and use only
  private temp directories and unnamed kernel objects; the support-log test is
  documented as safe for concurrent processes. They keep the 120 s timeout,
  and a failure fails the gate.

## Measured (after)

| Gate | Before | After |
|---|---|---|
| `check_support_diagnostics.py` | 147 s | 56 s (bounded by the 55 s support-log test) |
| `research_reference_checks.py` | 65 s | about 20 s |
| `run_native_backend_checks.py --require-compiler` | several minutes | 224 s cold, 83 s warm |
| `build_release.ps1` (all gates) | about 15 min reported by the owner | 179 s with a full MSVC rebuild, 90 s incremental |

The final EXE is identical to the previous candidate (SHA-256 `85666c24...`).

Note: run the gates from PowerShell or cmd (as `build_release.ps1` does).
MinGW test binaries started from Git Bash can segfault there, most likely
because Git's own older `libstdc++` DLL comes first on PATH. The same binary
passes from PowerShell, so this is not a test failure.

Backups: `.local/backups/before-fast-checks-2026-10-02/`.
