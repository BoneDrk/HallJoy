# MCHOSE: static survey of every M HUB firmware (2026-10-02)

Continuation of [Jet 75 II](MCHOSE_JET75_2026-10-02.md): the owner asked to
analyse all MCHOSE models. Input: the 33 official M HUB images and 70 web-driver
files downloaded under the owner's 2026-10-02 authorization
(`.local/research/mchose-mhub-20261002/`, nothing published). Static analysis
only: no device, no tester log, no runtime or Sheet change in this record.

Scripts (all in that folder): `survey.py` (architecture, RISC-V opcode table),
`family.py` / `family2.py` (RISC-V anchors, A0 descriptor and maximum tables),
`writer_facts.py` (`06` writer reload and reboot rule), `arm_family.py` /
`arm_cmp.py` / `arm_facts.py` (ARM images against Mix87 III 1.22),
`mkelf.py` (raw image → ELF for `llvm-objdump`). Outputs: `survey.json`,
`family.json`, `family2.json`, `family_keys.json`, `writer_facts.json`,
`arm_family.json`, `arm_facts.json`; listings in `dis/`.

## Result in one line

Every magnetic M HUB model with a published image uses the **same analog
design** as the two Supported boards: the `A0` key-travel report produced only
while **profile byte 7 bit 3** is set, behind the same `55` / `AA` command
transport. Three implementations of it exist, matching the two existing
backends plus one new chip.

## Group A: WCH RISC-V, base 0x5000 (Jet 75 II design)

All 11 images: opcode table `01..0E, A0..A9, (AA), B0, B1, DD, DE, EE, F1, F2`
(the survey's "base 0x0" for four of them was a detection miss; at 0x5000 the
anchors resolve), `04` reads 0x20200, `05` reads 0x20100, `06` writes ≤256
bytes at 0x20100 and reloads the active profile (stride 64) into config RAM,
the A0 service gate is exactly config RAM + 7 bit 3, and the A0 builder has the
same ≤9 → 0 / ≥max−5 → max clamp. Descriptor slots have no duplicate HID codes.

| Model (catalog) | VID:PID | FW | Keys (+Fn) | Max (0.01 mm) | Reload | Reboot after `06` if |
| --- | --- | --- | --- | --- | --- | --- |
| Jet 75 II (Supported) | 41E4:211A | 1.16 | 79+1 | 331/341/351 | 64 B | flash/reset flag set |
| Zero75X | 41E4:211C | 1.14 | 79+1, same set as Jet 75 II | 341 | 64 B | flash/reset flag set |
| Ace 68 Pro | 41E4:2116 | 1.21 | 67+1 | 341/351 | 64 B | not resolved by script |
| Ace 68 Air II | 41E4:2120 | 1.17 | 67+1 | 341/346 | 64 B | byte 4 low nibble changed |
| Ace 60 Pro | 41E4:2103 | 1.18 | 60+1 | 341/351 | 64 B | byte 4 low nibble changed |
| Ace 60 Pro Nordic | 3837:3002 | 1.07 | 61+1 (ISO key 0x64) | table not decoded | 64 B | byte 4 low nibble changed |
| Ace 60X II | 41E4:2112 | 1.04 | 60+1 | 330/341/351 | 64 B | byte 4 low nibble changed |
| Jet 75 I | 41E4:2118 | 1.09 | 79+1 | 341/351 | **32 B** | byte 4 low nibble changed |
| Ace 68 I | 41E4:2114 | 1.09 | 67+1 | 311/341/351 | **32 B** | byte 4 low nibble changed |
| Ace 60X I | 41E4:2126 | 1.05 | 60+1 | 311/341/351 | **32 B** | byte 4 low nibble changed |
| Mix 87 I | 41E4:2122 | 1.12 | 86+1 | 311/341/351 | **32 B** | byte 4 low nibble changed |

Notes:

- Older generation (32-byte reload): the reloaded part still contains byte 7,
  so the flag takes effect without a reboot. HallJoy writes only byte 7 bit 3,
  so the "byte 4 changed" reboot never triggers; the Jet 75 II "first write
  after flash/factory reset reboots" rule applies only to Jet 75 II and Zero75X.
- Switch maximums are inside the 200..600 range the Jet 75 II backend accepts.
- The 68/60-key images use 0x49 (Insert) and 0xE6 (Right Alt) slots; the
  60-key ones 0x65 (Application). Exact per-model layouts must come from the
  descriptor tables (already extracted) checked against the official
  `layout-*.js` files (present for all 32 catalog models).
- Ace 60 Pro Nordic: the switch-maximum table uses a different stride; not
  decoded yet (needs a manual look before use).

## Group B: ARM Thumb, base 0x08008000 (Mix87 III design)

Mix 87 III (Supported), Ace 68 III (3837:3003), Ace 68 Air III (41E4:2132),
Ace 68 Air 2 (3837:300A), Ace 68 V2 III (3837:3024), Ace 68 Turbo 8K
(3837:3028), Ace 75 8K (3837:303C). All have the same settings flash pages
(0x0802A000 base, 0x0802C000 settings), the `ldrb [cfg,#7]` bit 3 service gate
and the A0 builder. Builder versus Mix87 III over 150 instructions: identical
for Ace 68 Air III; first difference at instruction 87..130 for the others
(not yet classified: constants or logic). Service slot bounds: 92 (Mix87 III,
Ace 75 8K), 84 (Ace 68 III / Air III / V2 III / Turbo 8K), 76 (Air 2).

Ace 60 (41E4:2101, FW 1.20) is ARM too but at base 0x08004000 and a different
build: A0 marker at 0x0800B096 and a byte-7 bit-3 gate on config 0x20002AA2
were found; not compared further.

## Group C: new WCH core (Zba/Zbb + XW), dual image

Ace 68 GT (3837:3007, Feb 7 2026) and Ace 68 Turbo 16K (3837:3026,
Dec 12 2025), 432 KiB each. Disassemble with
`--mattr=+m,+a,+c,+f,+xwchc,+zba,+zbb,+zbs`. Found: the same `55` parser
(length ≤ 0x38, sum checksum, `AA`/`AB`, "unkw"), an extra `5F` frame type,
the M HUB opcode table `01..0E, A0..A9, B0..B2, DD, DE, EE, F1, F2` twice
(file 0xF394 and 0x5250C in GT; two linked copies), the A0 builder and a gate
on RAM 0x2017A217 bit 3 (= config + 7), with the same 80-slot round robin and
≤9 cutoff. Handlers `03..06` are not reviewed yet (runtime base of each copy
not established).

## Not analysed / no image

- No published image: Ace 60 Pro ISO FR (3837:3040), Ace 75 16K (3837:301D),
  GOD 60 (3837:3020), Jet 75 III (3837:3008).
- K87 V3 and its receiver: empty files on the CDN.
- G75 V2, G87 V2, G98 V3, K87S, K99 V3 and the 2.4 GHz receivers: different
  packaging (`37 38 <pid> <ver> AA BB` header) or a different image; magnetic
  sensing for these lines is not established and they are not in the Sheet.
- 19F5:FC30 / FC31 catalog records have no model or image.

## Sheet mapping (live rows, unchanged)

| Sheet row | Status now | Catalog devices behind it |
| --- | --- | --- |
| Ace 60 | Research incomplete | Ace 60 (ARM, group B-like) |
| Ace 60 Pro | Not investigated | Ace 60 Pro, Nordic (A); ISO FR (no image) |
| Ace 68 | No usable analog found | Ace 68 I (A), Pro (A), III (B), V2 III (B) |
| Ace 68 Air | Not investigated | Air II (A), Air III (B), Air 2 (B) |
| Ace 68 GT | Not investigated | GT (C) |
| Ace 68 Turbo | Not investigated | Turbo 8K (B), Turbo 16K (C) |
| Ace 68 V2 | Not investigated | V2 III (B) |
| Ace 75 | Not investigated | Ace 75 8K (B), 16K (no image) |
| GOD 60 | Not investigated | no image |
| Jet 75 (II revision) | Supported | — |
| Mix 87 (III revision) | Supported | — |

Not in the Sheet: Zero75X, Ace 60X I/II, Jet 75 I, Jet 75 III, Mix 87 I.

The red "Ace 68" outcome predates the A0/flag method: the historical capture
had PID 2116, which the catalog maps to **Ace 68 Pro**, not Ace 68 I
(see KEYBOARD_SHEET_MAGNETIC_AUDIT_2026-09-20.md). The new evidence does not
change that row by itself; it is an owner decision.

Owner-supplied log `HallJoyStabilityTrace.previous (2).log` (2026-10-02,
recorded in docs/firmware/mchose-ace68/TESTER_LOG_2026-08-29_PID2116.md):
the earlier session of the same attempt; again only `matched=0`, no command
was ever sent to `41E4:2116`. The 2116 updater's USB product string is
`Ace68-II` and retail units are sold as plain "Ace 68", so the Sheet row
"Ace 68" in practice means this device, and its red status rests on attempts
that never reached it.

## Outcome (same day)

- Group A (all 11 RISC-V images): implemented on the Jet 75 II backend, see
  [MCHOSE_RISCV_FAMILY_2026-10-02.md](MCHOSE_RISCV_FAMILY_2026-10-02.md)
  (and [MCHOSE_ACE68II_2026-10-02.md](MCHOSE_ACE68II_2026-10-02.md)).
- Group B (6 ARM images besides Mix 87 III): implemented on the Mix 87 III
  backend after emulator replay, see
  [MCHOSE_ARM_FAMILY_2026-10-02.md](MCHOSE_ARM_FAMILY_2026-10-02.md).
- Not implemented: Ace 60 (ARM, different build), group C (Ace 68 GT, Turbo
  16K: handlers 03..06 not reviewed), models without an image.
