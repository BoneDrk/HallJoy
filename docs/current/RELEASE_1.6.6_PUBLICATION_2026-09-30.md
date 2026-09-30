# HallJoy 1.6.6 publication — 2026-09-30

The owner explicitly authorized publication ("публикуй"). Title:
v1.6.6: MAD68 HE V2 Flagship support and shortcut fixes.

## Source

- The fresh publication checkout `.local/release166-final` was cloned from
  GitHub main 378f9fe8c341c23995d4e7fd033f06507929da05.
- An older, undocumented `.local/release166-publication` (2026-09-28, staged
  changes) was not used and was left untouched.
- Transferred changes: 106 modified files, 31 new files and 1 removed file
  (`block_keys_hotkey.h`). After the transfer, the checkout equals the
  workspace for every published path.
- Scan for private data: no new private data. The K4 serial and the
  `C:/Users/PC/Downloads` paths already appear in published docs.
- `check_publication_inputs.py`: PASS.
- Release commit: 66fe695049bda8edd62c403d0874eeb307a45b7e; tag v1.6.6.

## Scope

- MADLIONS MAD68 HE V2 Flagship is Supported; the tester confirmed it in game
  and after Pause/Resume.
- Unified shortcuts, working over remote access and from any keyboard.
- Pause/Resume re-claim fixes for Hex80, W669 and Addressed, including the
  Hex80 re-proof stop-flag fix.
- Curve reset and preset Revert buttons.
- AULA HERO reliability; AJAZZ rename; code-audit fixes.
- The K4 work is included in the source but, by owner decision, not in the
  patch notes.

## Validation

- In the clean checkout:
  - `run_native_backend_checks.py --require-compiler`: PASS.
  - `research_reference_checks.py`: public PASS; the private source replay is
    NOT RUN there.
- In the workspace, the full private record passed.
- The clean checkout cannot run `build_release.ps1`: the locally built UAP
  runtime dependencies are absent. As for 1.6.5, the EXE comes from the
  workspace build of identical sources:
  - `build_release.ps1` EXIT=0;
  - `--halljoy-require-full-catalog` exit 0;
  - FileVersion 1.6.6.0;
  - no source newer than the EXE.
- Sheet, from a fresh full native read (`.local/release166-sheet.json`):
  - structure audit PASS, 148 blocks;
  - `support_notice_catalog.py --sheet` PASS, 252 yellow.

## Published and verified

- https://github.com/PashOK7/HallJoy/releases/tag/v1.6.6 is stable and
  latest.
- Assets:

  | File | SHA256 |
  |---|---|
  | HallJoy.exe | 4c5887c5280f01120afe0f2442a0c650fb05c096b93273140a9e6e07c739e846 |
  | LICENSE | — |
  | THIRD_PARTY_NOTICES.md | — |
  | SHA256SUMS.txt | — |

  `SHA256SUMS.txt` uses the same format as 1.6.5.
- All four downloaded assets match the prepared files byte for byte.
- All 52 prior assets keep their IDs, names, sizes and creation dates, and
  their download counts did not decrease. There are now 56 assets.
- No visual or physical testing is claimed by the agent.

## Post-release CI fix (tests only)

- Hosted Linux portable CI failed on the release commit: `input_shortcuts_test`
  includes `keyboard_scan_hid.h`, which needs `windows.h` for `MapVirtualKeyW`.
  Local runs are on Windows, so this was not visible.
- Fix: the scan-code include and the hook-mapping block are under
  `#ifdef _WIN32`; the portable shortcut engine cases still run on Linux.
- Windows run: `INPUT_SHORTCUTS=PASS`.
- The released EXE and the v1.6.6 tag are unchanged. This matches the 1.6.5
  test-only follow-up.
