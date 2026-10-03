# IO and Pwnage: recheck for new firmware or knowledge (2026-10-02)

Owner question: can firmware be downloaded now, or is there anything new since
the previous reviews? Static checks only. Local files:
`.local/research/io-recheck-20261002/`, `.local/research/pwnage-recheck-20261002/`.

## IO

- `https://web.io.vision/update.json`: byte-identical to the 2026-09-15 /
  2026-09-30 copies (SHA-256 e7444529...). Type 68 Magnetic V1.37, Type 84
  Magnetic V1.17; still no Magnetic Pro firmware.
- Web configurator: same build (`main-CnGPaz9A.js`, `vendor-n6JO6zlV.js`,
  `layout-classic-DXDjowst.js`), so the frameVersion-1 `0x68` getter finding of
  IO_CATALOG_FIRMWARE_2026-09-30.md is unchanged.
- io.vision Yandex.Disk folder (`yd_tree.json`): keyboards are only SXT and
  Typex drivers (July 2025); newer files are mice/headsets. No keyboard firmware.
- Result: nothing new. Statuses unchanged.

## Pwnage

- Web Hub still `hub-3.4.4.min.js`, but the live file is 2,466,467 bytes
  (SHA-256 949c06a5...) instead of the pinned 2,466,514 bytes (94695F76...).
  No old copy is kept locally, so the 47-byte difference was not diffed.
  The new file has the same keyboard IDs (VID 3662, PID 1001/1002; other PIDs
  are mice) and no live-travel, ADC or calibration read (keyword scan).
- Community macOS app (github.com/edgetr/Zenblade-65-V2-MacOS-App): v0.1
  released 2026-09-26. Its protocol reads lighting, profile, keymap, SOCD and
  macros only; no live travel. Same IDs 3662:1001/1002.
- `pwnage.com/pages/drivers` timed out from this machine; new downloads there
  could not be checked. Zenblade 65 V2 firmware is still unavailable as far as
  checked. The support-correspondence follow-up (owner's mailbox) was not checked.
- Result: nothing actionable. Statuses unchanged.
