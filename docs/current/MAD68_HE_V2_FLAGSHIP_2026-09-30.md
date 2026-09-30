# MADLIONS MAD68 HE V2 Flagship — experimental native support (2026-09-30)

Owner request: build HallJoy with full support for the MAD68 HE V2 Flagship so
a tester can try it. Evidence: tester logs 49/50 show `373B:1125` present with
no analog source, and the firmware `MAD68HEV2_Flagship_V103.bin` (SHA256
c752a1d2…f95f, kept local in `.local/research/mad68-v2-flagship`). The
static analysis is in [FIRMWARE_TRIAGE_2026-09-30](FIRMWARE_TRIAGE_2026-09-30.md).
No device was used by the agent.

## Protocol (firmware-derived)

- Vendor interface `FF60:0061`, 32-byte IN/OUT reports, no report ID.
  Descriptor device `373B:1125`, product "MAD68 HE V2", Yizhita.
- The dispatcher (0x080195D8) accepts `02 96 <sub>` get, `03 96 <sub>` set and
  `96 96 96 …`. This is the ATK Hex80 framing.
- Get `0x1C` travel buffer (0x0801976C → 0x08018240):
  - offset is big-endian at bytes 5..6 and count ≤ 4 at byte 7;
  - from byte 8, each slot is u16 BE raw sensor (key struct +0), u16 BE
    travel (+2) and a status byte;
  - slot = row·15 + col, 75 slots; the firmware stops at slot 0x4A.
- Travel (+2) is written by the scanner from 0x08018E14, clamped to 330
  (0x14A). The actuation thresholds use the same units, 0.01 mm. Get `0x24`
  (the Hex80 travel-info scale) does not exist; the table ends at 0x20.
- The default layer at 0x0802A008 (u16, 5×15) has 68 keys; Fn is 0x5221.
  It matches the existing MADLIONS MAD68HE ANSI preset key set exactly.

## Implementation

- `hex80_protocol.h/.cpp` has a model table (`hex80::Model`) with
  `kHex80Model` and `kMad68V2Model`. `FindModel(pid)` does exact PID
  admission. The MAD68 V2 model uses:
  - `kMad68V2SlotToHid` (75 slots, Fn → 0x409);
  - a fixed scale of 330 and a raw deadzone of 3 (0.03 mm);
  - `calibrationFinish=false`.

  Hex80 behaviour is unchanged: 104 slots, `02 96 24` scale and `03 96 19`
  after proof.
- `hex80_backend.cpp`:
  - The candidate carries its model. `ProveProtocol` is a GET-only proof: the
    scale (queried, or the fixed one) plus one `02 96 1C` chunk.
  - No SET of any kind is sent to MAD68 V2.
  - Polling covers `model.slots`, and HID ownership is per model.
  - The layout token is `Token("hex80", model->layoutProduct)`.
  - HallJoy.log events: `hex80.probe` (value pid; data stage 0 ok, 1 open,
    2 travel info, 3 travel buffer), `hex80.session_ready` (pid, scale) and
    `hex80.session_lost` (pid, successful polls).
- Automatic layout:
  - `tools/prepare_madlions_layouts.py` gives the MAD68HE ANSI report the
    verified identity `hex80` / `MAD68HEV2-1125`. The report still reproduces
    from pinned sources.
  - Catalog sha and products were updated. `layout_pipeline` integrate
    regenerated 3 files, token 0xB8C12467F6B682BC.
  - `research_reference_checks.py --record`: all original checks passed.
- Yellow notice:
  - bit `Mad68V2 = 16777216`, counted in ImplementedModels;
  - title "MADLIONS MAD68 HE V2: hardware testing incomplete";
  - notice group `Mad68V2` (protocol 2, token-gated so ATK Hex80 is
    unaffected); generated header regenerated.

## Tests

- `hex80_protocol_test`:
  - model table and PIDs;
  - 68 unique keys, Fn;
  - row/col addressing;
  - every chunk including the last 3-slot chunk; out-of-range chunks rejected;
  - scale and deadzone normalization;
  - Hex80 map still separate.
- `hex80_routing_static_audit`: the fixed scale skips `02 96 24`, the
  calibration SET is Hex80-only, and ownership and the token are per model.

## Validation

- `run_native_backend_checks.py --require-compiler`: all static and portable
  C++ tests PASS.
- `research_reference_checks.py`: PASS after `--record`.
- `support_notice_catalog.py`: PASS, 253 models.
- `build_release.ps1` EXIT=0; `--halljoy-require-full-catalog` exit 0.
- Tester EXE: `build/bin/Release/x64/HallJoy.exe`, SHA256
  `d7be4ab58f84eccbb5e5696c58a1d32bd9cd4b884a9482d36018c8494d66423b`.
- No hardware run.

## Sheet (2026-09-30)

- A new row was inserted: Main!509, MADLIONS / "MAD68 HE V2 Flagship" /
  "Implemented; awaiting hardware testing".
- It is in alphabetical order before MAD68HE, as one revision-pinned
  batchUpdate:
  - height 32;
  - interior borders (left A, right C);
  - brand white on white;
  - B:C yellow base colors taken from the M4G row;
  - strict validation copied from the neighbouring row.
- Readback of A508:C510 and A509:C509:
  - the values, the full identical validation list and height 32 are correct;
  - userEntered and effective colors match;
  - no merges, no notes;
  - rowCount 1277 → 1278;
  - neighbours are unchanged.
- Not run: the full-sheet `check_keyboard_sheet_structure.py` and
  `support_notice_catalog.py --sheet` reconciliation. The connector returns
  grid data only into the conversation, not to a file, so a full bounded
  snapshot (1278 rows with per-cell validation) was not exported. Pending item:
  run both on a full native export at the next Sheet task with file export.

## Limits

- The key-to-matrix mapping and the scale come from firmware analysis only.
- Not verified:
  - direction and range on real switches;
  - typing while polling at full rate;
  - behaviour with the vendor configurator open.
- Wireless and receiver modes are not included.

## Promoted to Supported (2026-09-30)

The tester reports that everything works perfectly, including in game. After the
resume re-claim fix ([RESUME_RECLAIM_FIX_2026-09-30](RESUME_RECLAIM_FIX_2026-09-30.md)),
Pause/Resume also works. Owner decision: Supported.

- The `Mad68V2` yellow notice group, bit and title are removed; the notice
  header is regenerated with 252 yellow models.
- README: the model moved from the experimental list to the supported MADLIONS
  line. SUPPORTED_HARDWARE is updated.
- Live Sheet Main!510 "MAD68 HE V2 Flagship": Implemented; awaiting hardware
  testing → Supported. B:C use the green base colors of MAD68HE (row 511).
  Readback confirms strict validation, borders, the effective green and no
  notes.
- Included in release notes v1.6.6.

## Full Sheet audit before 1.6.6 (2026-09-30)

- A fresh native read was done with `get_spreadsheet` on Main!A1:C1279:
  - grid data, row metadata, user-entered and effective formats, validation;
  - rowCount 1279, no merges.
- The large result was saved by the client to a file and copied to
  `.local/release166-sheet.json`. The earlier "cannot export" note was wrong.
- `check_keyboard_sheet_structure.py`: PASS, 148 blocks, 0 issues. This
  includes the presentation checks.
- `support_notice_catalog.py --sheet` on the rows derived from that snapshot
  (`.local/release166-sheet-models.json`, 650 models): PASS, 252 yellow rows
  match the notices.
