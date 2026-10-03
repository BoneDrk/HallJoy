# tools/build.ps1 fixes (2026-10-02)

Owner question: "what is wrong with Hex80?" Nothing in Hex80 itself: the full
dependency build `tools/build.ps1` had two stale gates that the regular
`build_release.ps1` does not run.

## 1. Hex80 static preflight

- The preflight required the literal
  `!hex80::IsKnownProductId(candidate.attributes.ProductID)` in
  `hex80_backend.cpp`. Since the 2026-10-01 ATK family change the backend
  admits candidates with `candidate.model = hex80::FindModel(...)` followed by
  `if (!candidate.model) continue;` (same PID list; `IsKnownProductId` is a
  wrapper of `FindModel`). So `build.ps1` threw "Hex80 native protocol preflight
  failed" although the guard was intact.
- Fix: the regex now requires the current guard (model lookup AND the skip of
  unknown PIDs), so removing the guard still fails the build.
- Backup: `.local/backups/build.ps1.before-hex80-check-2026-10-02`.

## 2. Unused locals (warning C4189)

- `keyboard_subpages.cpp` WM_PAINT of the layout canvas kept `HBITMAP bmp` and
  `HGDIOBJ oldBmp` after the 2026-10-01 render-buffer change; `build.ps1`
  rejects any unexpected production warning.
- Fix: the two unused locals were removed (no behaviour change).
- Backup: `.local/backups/keyboard_subpages.cpp.before-unused-locals-2026-10-02`.

## Also seen

- The physical-UAP runtime gate requires every HallJoy instance to be closed;
  the installed elevated build was closed with `close_project_halljoy.ps1`
  (standing owner permission).
- After any `keyboard_support_notices.json` change the full build needs
  `research_reference_checks.py --record` first (as already documented for
  Jet 75).
