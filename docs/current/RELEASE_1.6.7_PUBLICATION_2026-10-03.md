# HallJoy 1.6.7 publication — 2026-10-03

The owner checked the new layouts visually and explicitly authorized
publication ("всё что надо от тебя делай и публикуй"). Title:
v1.6.7: Jet 75 II, Everglide SU75 Pro and RK68 HE support.

## Source

- Fresh publication checkout `.local/release167-final`, cloned from GitHub main
  672fcde1b4449b9dd3afaaa74672c9cf085c5630 with `core.autocrlf=false`.
- Main had one commit not in the workspace: the owner's README edit (672fcde,
  "written by chatgpt and claude"). It was ported into the workspace first, so
  publication kept it.
- Transfer (`.local/release167-final-diff.json`): 77 modified files and
  137 new files. Nothing was removed.
- Excluded (612 files), as in earlier releases:
  - every `denied_paths` entry;
  - IPI firmware updaters and archives;
  - the ATK hub JavaScript;
  - old `docs/firmware/mchose-ace68/backups/`;
  - the HERO84 baseline backup;
  - the Pwnage support correspondence.
- The gitignored fixture `tests/data/titan68_turbo_trace_valid.log` is still
  unpublished, as before.
- New JSON layout data from configurators (ATK hub keymaps, M HUB, RK, NuPhy,
  SteelSeries, illumiPC key lists, the SU75 Pro transcription) is published.
  This follows the precedent of the already published AULA, illumiPC, Keychron
  and ATTACK SHARK layout JSON. Raw vendor JS, images and firmware stay local.
- Private-data scan of the staged diff: no e-mails, user paths, serial numbers
  or tokens.
- `check_publication_inputs.py`: PASS.
- Release commit: 35d879fc53b2e0d793aa005eb6ddbc2b3ee962be; tag v1.6.7.

## Scope

- Supported: MCHOSE Jet 75 II, Everglide SU75 Pro, Royal Kludge RK68 HE.
- Experimental (yellow):
  - the ATK Hex80 family (19 Sheet rows);
  - 10 more MCHOSE models.
- New built-in layouts:
  - AK820 MAX HE, Mix 87 III, Field75 HE and Apex Pro;
  - the newly supported or experimental keyboards.
- MCHOSE fixes: no false missing-analog banner at idle start, and no
  disconnect on a device-change notification.
- The support log records USB product names.
- Faster local checks (internal), the build-close fix for an elevated HallJoy
  (internal) and the Alumix104 research work are not in the patch notes.

## Validation

- Workspace:
  - `build_release.ps1` EXIT=0 (161 s) with version 1.6.7.0;
  - `run_native_backend_checks.py --require-compiler` PASS;
  - no source newer than the EXE.
- Clean checkout:
  - `run_native_backend_checks.py --require-compiler` PASS (122 s);
  - research references: public PASS; the private source replay is NOT RUN
    there.
- Sheet, fresh full native read of `Main!A1:C1290`
  (`.local/release167-sheet.json`):
  - structure PASS, 149 blocks;
  - `support_notice_catalog.py --sheet` PASS, 281 yellow;
  - `check_supported_layouts.py --sheet` PASS, 70 Supported.
- Hosted CI on 35d879f: two "Native backend checks" runs, both success
  (37102882108, 37102884049).
- The owner did the visual check of the layouts. No agent visual or physical
  test is claimed.

## Published and verified

- https://github.com/PashOK7/HallJoy/releases/tag/v1.6.7 is stable and latest.
- Assets:

  | File | SHA256 |
  |---|---|
  | HallJoy.exe | 5b49adb287e5e65568a0bbfdbfeab46730138291d394ddffcd2c650dc79ccb50 |
  | LICENSE | 0d96a4ff68ad6d4b6f1f30f713b18d5184912ba8dd389f86aa7710db079abcb0 |
  | THIRD_PARTY_NOTICES.md | ee452aa6d3a3a48bc8ced4db698da08e2663dbec4819e35e0899397e4eab0d7f |
  | SHA256SUMS.txt | — |

  `SHA256SUMS.txt` uses the same format as 1.6.6.
- All four downloaded assets match the prepared files byte for byte.
- All 56 prior assets keep their IDs, names, sizes and creation dates, and
  their download counts did not decrease. There are now 60 assets.
- The commit has no Claude attribution trailer.
