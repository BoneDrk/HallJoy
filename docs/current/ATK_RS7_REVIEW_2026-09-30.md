# ATK RS7 family — protocol review (2026-09-30, no implementation)

Source: the pinned ATK hub bundle
`docs/research/remaining-layout-sources-20260914/atk-index-M8vylvoC.js` (hub
v3 3.2.25), the same source used for Hex80. The bundle is a 2026-09-14
snapshot: the current hub may list more PIDs. No firmware image was obtained;
the owner allowed proceeding without it. No device was used.

## Registry (VID 0x373B, vendor interface FF60:0061)

| Model | Provider / controller | Matrix | Wired PIDs (boot PID) |
|---|---|---|---|
| RS7 | DUCKBREAD / `Qln` | 6×16 | 0x1044 (boot 0x2002) |
| RS7 V2 / V2 Ultra / V2 EXTREME | BITYUAN / `rRt` (`iRt`) | 6×15 | 0x1174, 0x1197, 0x1198, 0x11A8, 0x11AD, 0x11AE, 0x11AF, 0x11B0, 0x11BA, 0x11DA, 0x11DF, 0x1336 |
| RS7 Air / Air Cyber | BITYUAN / `rRt` | 6×15 | 0x1220, 0x1221, 0x122B, 0x1257, 0x1258, 0x1295, 0x12C1, 0x1335, 0x1339 |
| RS7 Turbo | BITYUAN / `rRt` | 6×15 | 0x11E0, 0x11E1 |

"RS7 Pro" (a Sheet row) has no separate hub entry. It may be a retail name for
one of the above; this is unresolved. The travel table lists RS7 at 3.4 mm.

## Protocol

- Both controllers use the VIA-style custom command set (`nKt`/`Rin` enums):
  - `customId` = 150 (0x96);
  - `adcTripCompStatusBuffer` = 28 (0x1C);
  - `travel` = 36 (0x24);
  - `calibrationFinish` = 25 (0x19);
  - `dataReporting` = 20 (an unsolicited report the transport recognises).
- Record parser (`adcTripCompStatusList`): from payload byte 8, per slot u16 BE
  ADC, u16 BE travel × 0.001 mm and u8 status. This is exactly the
  `hex80_protocol` travel-buffer format.
- `getCalibrationStatus` in both `Qln` and `rRt` reads the whole matrix with
  `getAdcTripCompStatusBuffer(4*o, 4)`, the same 4-slot chunks as HallJoy
  Hex80.
- `rRt` (V2/Air/Turbo) also has a complete onboard gamepad API:
  `setGamepadEnabled`, `setGamepadMapping`, `setGamepadCurve`,
  `setGamepadSquareGate`, `setGamepadAngle`, `setGamepadMode`,
  `_fetchBityuanGamepadSnapshot` and others. This is a firmware XInput option
  similar to K4 onboard; it is unexplored.

## Feasibility

All RS7 generations fit the existing `hex80::Model` table (read-only
`02 96 1C`, scale from `02 96 24` in 0.001 mm, like Hex80). Each model needs:

- exact PIDs;
- a row/col slot map (RS7 96 slots, V2/Air/Turbo 90 slots) from hub
  geometry, or the device's default key matrix;
- a decision whether to send the Hex80 `03 96 19` calibration-finish (the same
  command exists in the enum).

Typing coexistence during polling is the same question already answered by
tester use of Hex80. It is not verified for RS7. The slot→HID maps are not yet
extracted.
