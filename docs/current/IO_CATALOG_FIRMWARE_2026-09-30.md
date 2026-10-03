# IO (IO by Red Square): catalog and firmware check (2026-09-30)

Owner request: check that every IO keyboard model is in the Sheet, download all
firmware, and check whether analog output was added. This supersedes the
2026-09-21 "catalog only, no firmware research" note for Type 68 Magnetic Pro
Wireless. Local evidence: `.local/research/io-2026-09-30` (manifest, web-driver
JS and SHA256 values in `sha256.txt`).

## Sources

- Firmware manifest `https://web.io.vision/update.json`: 18 entries,
  byte-identical to the 2026-09-15 copy
  (`docs/firmware/io-type84-magnetic/UPDATE_MANIFEST_2026-09-15.json`).
  - Type 68 Magnetic is still V1.37; Type 84 Magnetic is still V1.17.
  - The other entries are non-magnetic models: Type 68 SE, Type 68 Pro,
    Type 84 and Type 84 Pro Wireless.
  - No Magnetic Pro firmware is published.
- `io.vision` is a placeholder page. It links the web configurator and a
  Yandex.Disk folder. That folder has only Windows drivers for SXT and Typex
  (both use G3ms Sapphire mechanical switches), mice and headsets. It has no
  keyboard firmware.
- The web configurator (`web.io.vision`) is the shared driveall.cn platform.
  Its built-in device catalog (`src/config/keyboards/io/*`) has 32 entries.

## Magnetic models (the configurator's "performance" page)

| Configurator name | VID:PID | Firmware | Sheet |
|---|---|---|---|
| IO Type 68 Magnetic BL/WH | 0C45:0256 (598) | V1.37 | Type 68 Magnetic — Research incomplete |
| Type 68 Magnetic Pro BL/WH (+2.4G) | 0C45:80C9/80CA (32969/32970), 2.4G 0C45:FEFE | none | Type 68 Magnetic Pro Wireless — Not investigated |
| IO Type 84 Magnetic Black/White | 0C45:80D6 (32982) | V1.17 | Type 84 Magnetic — No usable analog found (FW 1.17) |
| Type 84 Magnetic Pro BL/WH (+2.4G) | 0C45:80D6/80D9 (32982/32985), 2.4G 0C45:FEFE | none | **missing** |

Retail check (DNS, Ozon, Wildberries, netbox): Type 68 Magnetic (io262) and
Type 84 Magnetic (io291/io292) are sold. No retail listing for either
"Magnetic Pro" was found. They exist in the vendor configurator only.

## New protocol in the configurator (frameVersion 1)

Framing: host `AA <cmd> <len> <addr lo> <addr hi> 00 <last> 00 <data>`,
device `55 <cmd> ...`. `GET_DEVICE_INFO` (0x10) byte 30 is `frameVersion`.

The new commands are:

- `0x60` GET_MAGNETIC_AXIS_KEY_STATUS: defined, not called by the UI.
- `0x68` GET_MAGNETIC_AXIS_STATUS: an **addressed multi-key read**.
  - The request carries up to 7 key values in 8-byte slots; the page address is
    `index * (reportCount - 8)`.
  - The reply gives, per key: calibration status (u8), ADC (u16 LE) and current
    stroke (u16 LE). The scale is 100 per mm, or 1000 when `rtPrecision == 2`.
- `0x69` / `0x6A`: calibration V2 on/off.

The UI calls `0x68` only on frameVersion-1 devices and only between
`0x69` and `0x6A`, cycling through all keys to show calibration progress. The
older `0x64..0x67` calibration and simulation stream (`55 FB`) is kept for
older firmware.

## Published firmware does not implement it

Disassembly of the `0x6*` dispatcher branch:

- Type 84 V1.17 at `0xDAF2`; Type 68 V1.37 at `0xDAC2`.
- Both handle only `64/65/66/67`. `60/68/69/6A` fall through to the generic
  echo reply (`0xDD36` / `0xDD06`).

So no published IO firmware has the new getter. Nothing changed since the
2026-09-15 review.

## Conclusions

- **No new analog in published IO firmware.** The Type 84 Magnetic result
  stands. For Type 68 Magnetic V1.37, the command branch matches Type 84: there
  is no multi-key getter.
  - The full Type 68 selection and serializer emulation was not repeated, so
    its status stays Research incomplete.
- The configurator shows that IO/driveall firmware with frameVersion 1 exposes
  an addressed multi-key read (`0x68`). This is the most likely route for the
  Magnetic Pro models, but no such firmware or device was available.
  - Unverified: whether `0x68` answers outside calibration V2, and whether
    calibration V2 affects typing or stored calibration.
- The Sheet is missing Type 84 Magnetic Pro (vendor configurator only; no
  retail listing found).
