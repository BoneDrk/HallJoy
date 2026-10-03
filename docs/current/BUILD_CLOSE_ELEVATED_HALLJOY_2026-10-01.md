# Build could not replace an elevated HallJoy (2026-10-01)

The owner reports that this keeps happening: the build stops because HallJoy is
open, although automatic closing was requested many times.

## Cause

Automatic closing already existed: `publish_halljoy_build.ps1` →
`close_project_halljoy.ps1`. It posts `HallJoy.ExitForBuild.v1`, waits, then
force-stops verified HallJoy processes.

It fails only when HallJoy runs **as administrator** and the build does not:

- UIPI drops the exit message from a lower integrity level. Only `ShowMessage`
  and `TaskbarMessage` were allowed with `ChangeWindowMessageFilterEx`.
- `Stop-Process` on an elevated process is denied: `CouldNotStopProcess`.

## Fix

- `app.cpp` WM_CREATE: `ChangeWindowMessageFilterEx(hwnd, ExitMessage(),
  MSGFLT_ALLOW)`. New builds accept a normal exit request from an unelevated
  build, with no prompt. Any same-desktop program can now ask HallJoy to exit
  normally, as it already could ask it to show its window.
- `close_project_halljoy.ps1`: when force-stop is denied and the script is not
  elevated, it repeats the same verified close once in an elevated PowerShell.
  Windows shows a UAC prompt. This is needed only for builds older than this
  fix that run as administrator.
- Verified on the owner's PC:
  - the elevated 1.6.6 instance was closed after one UAC prompt;
  - the new EXE was installed and the window restored;
  - EXE SHA256 `3afa5cfe9e8eaa94cd0621b682cfcaa54875fb6b8a7ef762cb42ecd93fbb7c08`.
- Also in this build:
  - the MADLIONS picker name "MAD68HE / MAD68R / MAD68 HE V2 Flagship";
  - `check_supported_layouts.py` now also requires that each Supported model
    can be found by name in the picker (`shownAs` for reviewed name
    differences). See SUPPORTED_LAYOUT_COVERAGE_2026-09-30.md.
- Checks:
  - `build_release.ps1` EXIT=0, `SUPPORTED_LAYOUTS=PASS`;
  - native checks PASS;
  - reference checks PASS.

## Note: one timing failure

One build attempt failed in the support-log contract test
(`await failed: analog_gamepad pub_while_connected=10`, 0xC0000409) while the
old HallJoy was running. The immediate rerun passed. It is unrelated to this
change; if it repeats, investigate the test's timing.
