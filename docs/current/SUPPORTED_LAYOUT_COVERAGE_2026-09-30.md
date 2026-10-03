# Layout coverage of Supported models (2026-09-30)

Owner question: are layouts added in HallJoy for all Supported keyboards?

## Method

- The Supported rows (67, any "Supported…" status) come from the fresh full
  Sheet read `.local/release166-sheet-models.json`.
- They were compared with the production-compiled layout catalog, exported by
  `tools/run_profile_transaction_tests.ps1`: 190 presets, saved as
  `.local/layout-catalog-2026-09-30.tsv`.
- The run also passed `LAYOUT_CATALOG` and `YELLOW_LAYOUT_AUDIT` (252 models,
  unchanged).

## Result

63 of 67 have a built-in preset:

- Sheet "IROK NA87" is NA87 Mag: preset "IROK NA87 Mag ANSI".
- MAD68 HE V2 Flagship uses "MADLIONS MAD68HE ANSI".
- IPI Aurora 75 and QBZ75 share one preset.

Missing exact layouts:

| Sheet row | Model | Notes |
|---|---|---|
| 17 | AJAZZ AK820 MAX HE (wired, RGB) | No AJAZZ AK820 preset. The backend does not publish a layout token. |
| 534 | MCHOSE Mix 87 (III revision) | MCHOSE_MIX87_LOG35: the exact visual layout was not added. |
| 592 | NuPhy Field75 HE | Only Air60 HE and Air75 HE have presets. |
| 698 | SteelSeries Apex Pro | STEELSERIES_APEX_PRO_IMPLEMENTATION: manual layout selection only. |

Before the fix below, these keyboards worked only with a manually selected
similar layout.

## Fix (2026-10-01): all four layouts added

The owner asked for every Supported keyboard to have a layout.
`tools/build_supported_gap_layouts.py` builds reviewed reports in
`docs/research/supported-gap-layout-reports/`. They are made from public JSON
extractions in `docs/research/supported-gap-layout-sources/`; the raw vendor
files stay in `.local`, and each extraction records the original URL and
SHA256. No vendor code was executed.

| Preset | Keys | Official source | Omitted | Automatic selection |
|---|---|---|---|---|
| AJAZZ AK820 MAX HE ANSI | 82 | `illumipc.com/config/keys/SG8994HERGB.json` | rotary knob | yes: `ajazz-m484` / `SG8994HERGB`, published only in a proven AJAZZ session |
| MCHOSE Mix 87 III ANSI | 87 | M HUB `CZ_SHARED_DATA` layout chunk (Mix87 III reuses Mix87 module 654) | G1/RT/Light buttons; Fn is display-only | yes: `mchose-mix87` / `MIX87III-3837-300D`, after firmware admission |
| NuPhy Field75 HE ANSI | 82 | `drive.nuphy.io` main.23dc78ef.js, Field75HE keys | G1..G8 (the driver excludes them) and the screenshot macro | no, as for Air60/Air75 HE (UAP session) |
| SteelSeries Apex Pro ANSI | 104 | GG 120.0.0 `app.asar` `render/index.css` rules (`.code<HID>`, US region) | Menu zone that GG draws over the arrows (not in the firmware map); F0 is shown as Fn | yes: `steelseries-apex` / `APEXPRO-1038-1610`, original 1038:1610 only |

- The rows of each layout line up on the right. Apex Pro keeps GG's small left
  offset of the Tab/Caps/Shift/Ctrl column.
- `layout_pipeline.py integrate` updated 12 generated files; the backup is
  `.local/backups/layout-integrate-gh2erd77`.
- Backends changed: `irok_na87_backend.cpp` (AJAZZ token, and its self-test
  expects it), `mchose_mix87_backend.cpp` and `steelseries_apex_backend.cpp`.
  Backup: `.local/backups/src-before-layout-tokens-2026-10-01.tgz`.
- `tools/tests/test_layout_pipeline.py`: the NuPhy count now includes Field75.
- The owner assesses the visuals; the agent did not do a visual run.

## Rule: no Supported without a layout (owner, 2026-10-01)

- `docs/development/supported_layouts.json` lists all 67 Supported Sheet models
  and their built-in presets.
- `tools/check_supported_layouts.py <layout-catalog.tsv>` fails if a listed
  model has no preset in the compiled EXE. It runs in
  `run_profile_transaction_tests.ps1`, which both `build.ps1` and
  `build_release.ps1` now run.
- `--sheet <fresh snapshot>` fails if a Supported Sheet row is missing from the
  list, or a listed model is no longer Supported.
- On the 2026-09-30 catalog the check reports exactly the four gaps above. With
  the live Sheet it passes (67).
- Written into KEYBOARD_SHEET_RULES.md, SUPPORT_STATUS_SYNC.md and AGENTS.md.

## Validation (2026-10-01)

- `build_supported_gap_layouts.py`: 4 reports PASS, byte-identical.
- `layout_pipeline.py check` passes for AJAZZ, NuPhy, MCHOSE and SteelSeries.
- `research_layout_checks.py` PASS. `research_reference_checks.py --record`:
  all original checks passed.
- `check_publication_inputs` rules: no violation for the 11 new files.
- `build_release.ps1` EXIT=0, including:
  - `--halljoy-na87-native-self-test` (AJAZZ token);
  - `PROFILE_TRANSACTION_WINDOWS_TEST=PASS`, `LAYOUT_CATALOG=PASS`,
    `YELLOW_LAYOUT_AUDIT=PASS`;
  - `SUPPORTED_LAYOUTS=PASS models=67`.
- `run_native_backend_checks.py --require-compiler`: PASS.
- EXE SHA256 `9cdfe51664649a63c81585fc698ba981ba70a8203ccb9f05d839b33a248e3824`.
- `check_supported_layouts.py --sheet .local/release166-sheet.json`: PASS.
