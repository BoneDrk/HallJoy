# MCHOSE Ace 68 (Ace68-II, 41E4:2116): tester build (2026-10-02)

Owner request: build HallJoy for the tester who owns the MCHOSE Ace 68 behind
the earlier failed attempts (PID `2116`, see
[family survey](MCHOSE_FAMILY_SURVEY_2026-10-02.md) and
docs/firmware/mchose-ace68/TESTER_LOG_2026-08-29_PID2116.md).

## Identity

- `41E4:2116`: M HUB catalog storage key "Ace 68 Pro" (type 112, firmware
  121, boot `41E4:2117`); USB product string `Ace68-II`; sold as plain
  "Ace 68". HallJoy shows "MCHOSE Ace 68 (Ace68-II)".
- Not covered: Ace 68 I (`41E4:2114`), Ace 68 III / V2 III / Air / Turbo / GT
  (other PIDs and, for the ARM ones, a different implementation).

## Firmware review: stock 1.21 against Jet 75 II 1.16

`update_ace68pro.4ed9aa6d_562fcc8c87ad.bin`, WCH RISC-V, **load base 0x5000**.
The 2026-08-29 notes (docs/firmware/mchose-ace68-pro/DEEP_REVERSE.md) treated
file offsets as runtime addresses; that is why they found no opcode table and
classified the A0 code as unrelated. At base 0x5000 everything resolves:

| Point the backend relies on | Jet 75 II 1.16 | Ace68-II 1.21 |
| --- | --- | --- |
| Opcode table | 0x13840 | 0xFCB0, same opcode set |
| `03` reply | 0x0116 + "Aug 28 2025,10:45:06", length 22 | 0x0121 + "Nov  8 2025,15:59:23", length 22 |
| `04` / `05` | read 0x20200 / 0x20100 + offset | identical code |
| `06` writer | ≤256 B at 0x20100, reload 64 B of the active profile | same; reload into 0x20004F54 |
| Reboot after `06` | if settings byte 0 bit 0 set | same test (`0x100(s1)` bit 0) |
| Factory settings | 4 × 64 B, `AA BB` at 2..3, byte 7 = 0x06 | same (present in the image at 0x20100) |
| Byte 7 bit 3 readers | only the A0 service | only the A0 service (other reads test bits 1/2) |
| A0 builder | travel 6..7, maximum 14..15, ≤9 → 0, ≥max−5 → max | same stores; maximum table stride 12 |
| Service | 80 slots round robin, gate config+7 bit 3 | same, gate 0x20004F5B |
| Keys | 79 + Fn | 67 + Fn in 68 of 80 slots, no duplicates |
| Switch maximum | 331/341/351 | 341/351 |

The official layout module (Ace68 module 76340, re-exported for this PID) has
exactly the firmware's 67 HID keys + Fn; the layout build checks this.

## Implementation

- `mchose_jet75_protocol.h`: model table `Models[]` (Jet 75 II, Ace68-II) with
  VID/PID, reviewed version, names, layout identity and key list;
  `FindModel`, `Allowed(model)`. Transaction code unchanged.
- `mchose_jet75_backend.cpp` (protocol 29): enumerates both PIDs; the session
  uses the candidate's model for admission log (`jet75.model`,
  `jet75.firmware_version`, `jet75.firmware_unreviewed`), routing claim, key
  set, telemetry name/IDs and layout token. Same lifecycle as Jet 75 II: flag
  on at start/resume, off at pause/exit, one automatic enable per generation,
  no version binding.
- Layout: `build_supported_gap_layouts.py` source `mhub-ace68` → report
  `mchose_ace_68_ansi` (68 keys) → token `mchose-ace68` /
  `ACE68II-41E4-2116`, automatic after admission. `layout_pipeline.py
  integrate MCHOSE` (backup `.local/backups/layout-integrate-but85pj4`).
- Yellow notice: `FrozenModel::MchoseAce68` (33554432), title
  "MCHOSE Ace 68: hardware testing incomplete", notice group protocol 29 with
  the Ace 68 token only (Jet 75 II stays without a notice).
- README experimental list and SUPPORTED_HARDWARE row added.
- Backup of all touched files: `.local/backups/before-ace68ii-2026-10-02-ace68ii.tgz`.

## Verification (agent)

- `mchose_jet75_protocol_test`: previous 2048 transactions plus the model
  table (exact PIDs, 67 keys, no duplicates, Insert/Right Alt decode, no
  cross-model keys): PASS.
- `mchose_jet75_session_test`: all previous scenarios plus an Ace68-II session
  (1.21 reply, enable, stream, own keys, telemetry 41E4:2116, 67 keys, layout
  token, cleanup): PASS.
- `support_notice_catalog.py`, `build_supported_gap_layouts.py`,
  `layout_pipeline.py check MCHOSE`, `research_reference_checks.py --record`
  (all original private audits passed): PASS.
- `build_release.ps1`: PASS (profile/recovery gates, `SUPPORTED_LAYOUTS=PASS
  models=68`, `YELLOW_LAYOUT_AUDIT=PASS 272`, `LAYOUT_CATALOG=PASS`);
  `check_support_diagnostics.py`: `SUPPORT_DIAGNOSTICS_RELEASE_GATE=PASS`;
  `run_native_backend_checks.py --require-compiler`: all static and portable
  tests PASS.
- Tester EXE: `build/bin/Release/x64/HallJoy.exe`, SHA-256
  `60d634ead38246d6dd3e7d7fa8d833dd3af1a6c98ded56fc136c2e3dbf75a151`.
- No physical test.

## Not done

- Live Sheet: the row "Ace 68" is still red "No usable analog found". Changing
  it to "Implemented; awaiting hardware testing" is left to the owner
  (the notice catalog lists the model as implemented; `--sheet` will report the
  difference until then).
- `tools/build.ps1` (full dependency rebuild) currently fails its static Hex80
  text check: since the 2026-10-01 ATK change the backend calls
  `hex80::FindModel` instead of `!hex80::IsKnownProductId(...)`. Not related to
  this change; the regular `build_release.ps1` was used.
