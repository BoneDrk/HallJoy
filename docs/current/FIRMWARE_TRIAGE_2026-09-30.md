# Firmware and log triage: HERO84 HE V2.16, MAD68 HE V2 Flagship, Atom HE68 — 2026-09-30

Source: owner folder `Downloads/Claude/Claude`. It holds three tester logs
(HallJoy 1.6.5.0), the AULA updater `Update_KB_2829SE_0.01_B08_8K_ARGB_32K_20250922_V2.16`,
`MAD68HEV2_Flagship_V103.bin` and `Atom_HE68_Firmware_update_V1.0.exe`.
Copies are under `.local/research/{aula-2829se,mad68-v2-flagship,atom-he68}`.
Nothing was executed and no device was used. This is static analysis only.

## AULA HERO84 HE V2.16 (tester reports "does not work"; no log yet)

- The updater embeds `firmware.bin`, SHA256 dbef78cf…6faf, 164584 bytes.
  `config.json` gives `GEEHY_USB_V2`, `372e:103e`, version `0216`, pass
  `110000000005` and `device_usb24g` (the 2.4G receiver is `372E:1000`).
- Compared with the analysed V2.22 image (750F1A9D…):
  - Same vector and reset address. The same `FF60:0061` descriptor sits at the
    same offset, and `372E:103E` appears at the same offsets.
  - The UUID `110000000005` is present.
  - The `94` dispatcher is at 0x0801D40C (V2.22: 0x0801D464) and has six
    subcommands.
  - The `94 02` handler is instruction-for-instruction the same loop. It uses
    the same position lookup and a byte-identical filtered-current reader
    (0x080127C0), and writes the same 6-byte record.
- Conclusion: V2.16 wired should be admitted by the existing exact-identity
  backend. The protocol difference is not the cause.
- Likely causes, until the log arrives:
  1. Use over the 2.4G receiver or Bluetooth. The backend is wired-only by design.
  2. Another program holds or disturbs the vendor interface, or an unsolicited
     or stale report breaks the single-read exchange (fixed below).
  3. One lost identity reply at engine start. The previous code made a single
     attempt and did not claim the keyboard for the whole session (fixed below).
  4. The adaptive range treats the first shallow press as the full range. This
     is a known limitation and unchanged.

Host fix in `aula_hero84he_backend.cpp`:
- `Exchange` flushes the input queue before the request. It then skips foreign
  reports until the reply matches (identity, `83` map, `94 02` records) within
  the 200 ms timeout, instead of failing or mis-correlating.
- Identity and map reads are retried 3 times.
- HallJoy.log now carries the reason (deduplicated). The stage is one of:
  open (Win32 error), identity (foreign-report count), unknown model (UUID),
  map (batch offset), or session lost.

  | Event | Meaning |
  | --- | --- |
  | `hero84.not_ready` | not-ready stage, as above |
  | `hero84.session_ready` | mapped keys, bcdDevice |
  | `hero84.no_exact_interface` | 103E interfaces without the exact FF60/report-09 shape; other 372E product ID seen, e.g. the receiver |

- Static audit updated. Protocol and the command allow-list are unchanged.

## MAD68 HE V2 Flagship V103 — logs 49/50 (`373B:1125`, plus `372E:1019`)

- The image has a 14-byte header (`373B:1125`, `373B:1021`, version bytes);
  the vectors follow at file offset 14, which is address 0x08010000.
  Strings: "Shenzhen Yizhita Technology Co., Ltd", "MAD68 HE V2". The vendor
  interface is `FF60:0061`, 32-byte IN/OUT, with no report ID.
- The vendor dispatcher at 0x080195D8 accepts `02 96 <sub>` (get, sub
  0x00..0x20), `03 96 <sub>` (set) and `96 96 96 …`. This is the Hex80 framing
  (`kGetValue 0x02`, `kCustomCommand 0x96`).
- Get `0x1C` (0x0801976C):
  - Offset is big-endian at bytes 5..6 and count ≤ 4 at byte 7. Records start
    at byte 8, 5 bytes per slot: u16 BE raw sensor (struct +0), u16 BE travel
    (+2), status byte. This is exactly the Hex80 travel-buffer format.
  - The matrix is 15 columns by 5 rows (75 slots, offset ≤ 0x4A). Hex80 uses
    17 columns and 104 slots.
  - Travel is written by the scanner (0x08013DE6) from 0x08018E14, clamped at
    330 (0x14A); the actuation thresholds are compared in the same units. This
    means 0.01 mm with ~3.30 mm full travel.
  - Get `0x24` (Hex80 travel info) does not exist; the table ends at 0x20.
    Scale 330 comes from the firmware constant.
- Default keymaps (5×15, u16) are at 0x0802A008 and 0x0802A134. There are 68
  keys; Fn = 0x5221/0x5223.
- The existing backends do not match it:
  - MAD68 Pro R: `373B:1109`, A0 protocol.
  - Hex80: `373B:1176/1177/1250`, 104 slots.
  - V2 Dual trial: `28E9:3265`.
- Option: a Hex80-family variant for `373B:1125` with its own slot map (from
  the firmware keymap), 75 slots and a fixed 330 scale. The request is
  read-only (get `0x1C`). Remaining unknowns are typing coexistence while
  polling at rate, and the exact physical range. This needs owner approval as a
  new admission, followed by the support-status sync.
- `372E:1019` is not in any catalog or image examined; its product is unknown.

## Fantech Atom HE68 (updater V1.0) — log 48 (`30FA:1440`, `2E3C:C365`)

- The updater is the SONiX USB MCU ISP Tool. The chip is SN34F280, and ISP IDs
  include `0C45:8802/8037/809E` and `38A6:2712`. The image comes from
  `SN34F280_User.hex`, 0x0000..0x7DFFF; the binary SHA256 is 66591c33…ed27.
  RW data is compressed by the linker, so the runtime VID/PID are not in
  plain text.
- The vendor interface is `FF68:0061`, the same shape as AULA Mini60 HE Pro and
  Alumix104. Two `AA`→`55` command handlers exist (0x00013270, 0x00014B48):
  - `10..18`: configuration pages 0x9000/0x9200/0x9600, as in the Mini60
    framework.
  - `21..2A`: configuration.
  - `32/33`: per-key RGB set/get.
  - No `66/67` analog-stream enable and no `FB` depth record.
- Result so far: no host-readable analog path was found in the examined
  dispatchers. This is not a proof of absence. Next steps would be to
  decompress the RW region (to get the runtime IDs and descriptors) and to
  check other interfaces and a gamepad descriptor.
- Deeper pass, 2026-09-30, owner asked for memory reads, addressed key reads
  or anything else:
  - Every reader of both receive buffers (0x2000D751, 0x20009C50) was
    enumerated. Both handlers accept only `AA` frames. Byte 1 is one of:
    - `10..18` read and `21..2A` write, over fixed flash pages at
      0x9000..0xBE00;
    - `13` device info;
    - `32/33/35` per-key RGB / RGB565;
    - `?F` erase and rewrite of configuration pages (factory reset — a write,
      never to be sent);
    - `6x` writes (it shares the `2x` path).
  - The page read address is base + 16-bit offset (bytes 3..4) and is not
    bounds-checked. It can still reach only flash up to 0x1AFFF, never RAM
    (0x2000_0000) where the scanner state lives. It is not a memory-read
    primitive for live values.
  - The transmit routine (0x1A644) sends:
    - EP0: 8-byte boot keyboard;
    - 16-byte NKRO;
    - EP1: report IDs 3 (3 B), 2 (2 B) and 6 (6 B, built from key/action
      bitmaps);
    - EP2: 64-byte vendor replies (only after an `AA` request).
  - There is no gamepad report, no unsolicited depth stream, and no per-key
    or addressed travel query.
  - Conclusion: the stock V1.0 firmware exposes no host-readable analog
    value. Remaining theoretical routes are custom firmware for SN34F280 (large
    effort, flashing risk) or another firmware version. Not recommended to
    continue without new material, such as a configurator that shows live
    travel.
- Log 48: `2E3C:C365` is present but W669 did not admit it. That backend
  requires exact firmware-product identity and never selects by the shared
  VID/PID. The product is unknown from this log. `30FA:1440` is unidentified.

## Sheet sync (2026-09-30, owner request)

The owner connected the Google Sheets connector (native API).
- Target: spreadsheet "HallJoy supported keyboards", tab Main (sheetId 0),
  1277 rows.
- Rows were located by brand and model:
  - Main!290 is Fantech ATOM HE68 (MK811);
  - Main!291 is ATOM HE68 Pro (MK922).
- Change: C290 `Not investigated` → `No usable analog found`, an existing
  choice of the strict dropdown.
- Base colors of B290:C290 were materialized as the red status style copied
  from Main!372 (IO Type 84): background (.95686275, .7176471, .7176471), text
  (.47843137, .1254902, .1254902).
- The write was one batchUpdate pinned to the read revision.
- Readback of A289:C292:
  - the value is correct and strict validation is kept;
  - userEntered and effective colors are red in B:C;
  - the brand cell and separators are unchanged;
  - MK922 still reads `Not investigated`, grey;
  - no notes.
- No rows were inserted and no yellow rows changed, so the structure and notice
  audits are not required.
- Not changed: ATOM HE68 Pro (MK922). The analysed updater "Atom_HE68 V1.0"
  does not prove Pro firmware.

## Validation

Native backend checks (compiler): PASS, including the updated HERO84 static
audit. `build_release.ps1` EXIT=0; `--halljoy-require-full-catalog` exit 0.
Installed `build/bin/Release/x64/HallJoy.exe` SHA256
`e35a3f9a0e5eab7c46bad45beda4f37a14e1378cbfcd6f2d7561c0c2d91d921f`. No hardware
run. HallJoy runtime support is unchanged; the only Sheet change is the
ATOM HE68 (MK811) research status above.
