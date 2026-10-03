# ATK keyboards on the Hex80 protocol — experimental support (2026-10-01)

Owner request: give a yellow support level to every ATK keyboard that works like
Hex80. "We do everything we can so these keyboards work fully; there is nothing
to test on yet, but this saves time when a real user appears." No device was
used. Supersedes the "no implementation" stance of ATK_RS7_REVIEW_2026-09-30.md.

## Source

Official ATK hub `hub-v3/production/3.2.27`:

- bundle `static/index-F-MYsmWR.js` (SHA256 275ec10d…f394256), kept in `.local`;
- `keymaps/<model>.json` and `demo/rs6.json`.

`tools/build_atk_hex80_family.py`:

- It builds 15 models and 10 merged layout reports.
- `--extract-facts` writes `hub-facts.json` (PIDs, names, controller, matrix)
  from the pinned bundle.
- The default run builds 15 layout reports and the generated header
  `src/HallJoyProject/HallJoy/generated/atk_hex80_family.h`, and checks them
  byte for byte.
- Public inputs are in `docs/research/atk-family-sources/`. Before publication,
  check their redistribution permission (AGENTS.md "Distribution provenance").

## Which models use the Hex80 protocol

The hub reads the `02 96 1C` adc/travel/status buffer in 4-slot chunks
(`getCalibrationStatus`) in two controller classes:

- **BITYUAN** (the Hex80 class): travel in 0.001 mm, scale via `02 96 24`.
- **DUCKBREAD**: travel in 0.01 mm, no travel-info command.

Hex80 itself is in the BITYUAN list.

| Sheet rows | Hub model (keymap) | Matrix | Controller | Key identities |
|---|---|---|---|---|
| EDGE 60 HE | kb_60 | 5×14 | BITYUAN | hub factory defaults |
| 60 RX (new row) | atk60_rx | 5×14 | BITYUAN | hub factory defaults |
| EDGE 63 HE | kb_63 | 5×14 | BITYUAN | hub factory defaults |
| RS63 Air | atk63_rx | 5×14 | BITYUAN | identical geometry to kb_63 |
| RS6, RS6 Ultra (and "Nothing 68" PIDs) | kb_68 | 5×15 | BITYUAN | **RS6 device dump** (`demo/rs6.json`, PID 0x109C) |
| 68 V3, RS6+, RS6 Ultra+ | atk_68 | 5×15 | BITYUAN | hub defaults (match the RS6 dump except the Ins/Del order) |
| RS6 Cube (new row) | atk_68_v4 | 5×15 | BITYUAN | identical geometry to atk_68 |
| 68 RX | atk68_rx | 5×15 | BITYUAN | hub defaults |
| RS6 Air (incl. Cyber) | rs6_air | 5×15 | BITYUAN | hub defaults (identical to the RS6 dump) |
| EDGE 75 HE | edge75 (+edge75_v2) | 6×15 | BITYUAN | standard 75% position |
| RS7 V2, RS7 V2 Ultra, RS7 Air | rs7_v2 | 6×15 | BITYUAN | standard 75% position |
| RS7 Air (ISO PID 0x12C1) | rs7_air_iso | 6×15 | BITYUAN | standard 75% ISO position |
| RS7 Turbo | rs7_revi | 6×15 | BITYUAN | standard 75% position |
| RS7 | rs7 | 6×16 | DUCKBREAD | hub defaults; fixed 3.40 mm |
| 68 V2 Pro | atk68_v2_pro | 5×15 | DUCKBREAD | hub defaults; fixed 3.30 mm |

### Matrix stride

slot = row × cols, using the hub **device filter** `custom.row/col`, not the
keymap `matrix` field. The filter gives 6×17 for Hex80, exactly the
hardware-proven HallJoy map; the keymap says 5×15, which is stale. Every keymap
index fits inside its filter matrix.

### Reliability of key identities

- Hub keymap default actions are reliable for the 60/63/68 boards and RS7, and
  were checked against the RS6 device dump.
- They are **not** reliable for Hex80, where 80/88 differ from the dump, nor for
  the 75% BITYUAN boards, whose defaults are a 68% template shifted one row.
- For those 75% boards each key gets its standard ANSI/ISO position in the
  official geometry. Every special width is asserted (Backspace 2u, Tab 1.5u,
  Caps 1.75u, Shift 2u/1.25u ISO, Space 7u).
- The right column follows the original RS7: Del, Ins, PgUp, PgDn; End on
  EDGE 75.
- The extra top-row key at [0,13] on EDGE 75, RS7 V2/Air and Air ISO has no
  reliable identity. It is left unmapped, which leaves a gap in the layout.
- A wrong guess for a navigation key mislabels only that key; the bound
  position still works.

## Implementation

- `hex80::Model` now holds a PID pointer and count. `kModels` is Hex80 +
  MAD68 HE V2 + the 15 generated ATK models.
- Same read-only GET proof and polling as Hex80; **no SET of any kind** is sent
  to the new models (`calibrationFinish=false`).
- Owned-HID tables are per model (`kOwnedByModel`). The layout token is
  `Token("hex80", product)`.
- Automatic layouts: 10 presets (ATK brand) selected from the verified session
  token. The layout catalog audit forbids identical same-brand duplicates, so
  boards with identical geometry and keys share one preset. Every model keeps
  its own identity token.
  - EDGE 60 HE + 60 RX;
  - EDGE 63 HE + RS63 Air;
  - RS6 + RS6 Ultra + RS6 Air;
  - 68 V3 + RS6+ + RS6 Ultra+ + RS6 Cube + 68 RX.

  Internal names use " + "; the picker shows " / " as for other merged
  layouts.
- Yellow notice: group `AtkHex80Family` (bit 16777216, protocol 2, gated by the
  15 tokens, so Hex80 and MAD68 are unaffected). Title "ATK: hardware testing
  incomplete". 19 Sheet models.
- Not included:
  - ATK 68 (GTECH);
  - 68 V2 (ARBITER controller: a different protocol);
  - 68 V2 S, 75 and RS7 Pro (no hub entry);
  - Velota/Fuzzy63 (not ATK).

## Tests and validation

- `hex80_protocol_test` checks:
  - every PID resolves to its own model, and no PID is shared;
  - there are no duplicate HIDs, and the key counts are as expected;
  - anchor slots (W, Space, Fn) are right for each stride (5×14, 5×15, 6×15,
    6×16);
  - every chunk decodes, including the short last one;
  - the DUCKBREAD fixed scale;
  - ATK68 V2 (ARBITER) is not admitted.
- Hex80 static audits PASS.
- Sheet (one revision-pinned batch, 31 requests):
  - 17 rows set to "Implemented; awaiting hardware testing";
  - rows "60 RX" and "RS6 Cube" inserted;
  - readback: structure PASS (148 blocks), all 652 rows equal the plan,
    `support_notice_catalog --sheet` PASS (271 yellow),
    `check_supported_layouts --sheet` PASS.
- Snapshot: `.local/atk-family-sheet-after.json`.

## Limits

- Typing while polling, real travel range and switch variants are not verified
  for any new model.
- Wireless and receiver modes are not included.
- The onboard gamepad mode of the BITYUAN boards is not used.

## Build (2026-10-01)

- The first release build failed in `audit_layout_catalog.py`: four groups of
  identical same-brand ATK layouts. They were merged into shared presets (see
  above) and the stale exports were removed.
- `research_reference_checks.py --record`: all original checks passed.
- `run_native_backend_checks.py --require-compiler`: PASS.
- `build_release.ps1` EXIT=0:
  - `LAYOUT_CATALOG=PASS`, `YELLOW_LAYOUT_AUDIT=PASS 271`,
    `SUPPORTED_LAYOUTS=PASS models=67`;
  - the running HallJoy was closed and restored without a UAC prompt.
- EXE SHA256 `a99450a77b58b3ce0e83333981c80a4c68fdfeeb65d2956191c93ac772d70886`.
