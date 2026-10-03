# K4 HE onboard: freeze on Block toggle and Pause — 2026-09-29 (r8 flashed)

Owner report: pressing the Block Bound Keys shortcut freezes analog for a couple
of seconds; Pause shows the same unjustified freeze.

## Causes (code + debug log evidence)

1. Block toggle changes `HJO_SUPPRESS`, so the host re-uploaded the whole 5044-byte
   onboard profile: ~241 A9/73 exchanges plus heartbeats. Measured K4 exchange
   rate (~160 depth snapshots/s) puts this at roughly 1–2 s. The worker reads no
   depth meanwhile, so the preview and analog shortcuts stall. Firmware kept
   driving the pad from the old profile. Curve edits cause the same full
   re-uploads.
2. Pause stops the K4 worker. STOP (or the lease watchdog) turns off the native
   descriptor, so the whole keyboard re-enumerates USB. The host then waits for
   the ordinary device (up to 3.5 s). Resume re-enumerates again and uploads the
   full profile again. The debug log shows 1.77 s in `StoppingProviders`.
   Typing is lost during each re-enumeration too.

## Fix: firmware r8 + host

Shared header `keychron_onboard_session.h`:
- Phase `HJO_PARKED`: keeps the native descriptor, neutral pad, no suppression,
  no owner. Its lease is `HJO_PARK_LEASE_MS` (30 min).
- `hjo_host_park`: current generation only; an active pad is neutralised.
  `hjo_open` accepts PARKED (no mode change).
- Capabilities `HJO_CAP_DELTA_UPLOAD` (64) and `HJO_CAP_PARK` (128).

Firmware `halljoy_onboard.c` (r8, includes r7 Alt/Tab):
- A9/72 byte8=1 is a delta BEGIN. The base CRC is at bytes 12..16. It is
  accepted only while staging holds exactly the committed wire
  (`staging_committed`); the received bitmap is then pre-filled.
  Capture, full BEGIN and delta BEGIN clear `staging_committed`. Commit sets it.
- A9/77 byte8=1 is PARK. Neutral is delivered, then `neutral_pending` clears.
  CHUNK, COMMIT and BEGIN are refused while parked.

Host:
- `Client::Upload` sends only changed 21-byte chunks, falling back to a full
  upload when the firmware refuses the delta. A Block toggle is 2 chunks (flag +
  CRC). `CommittedProfile` survives worker restarts.
- `Client::Park`: two tries (to skip a stale reply), then an orderly STOP.
  `Open` from PARKED skips `Reconnect`. `ReleaseParked` handles exit.
- Backend:
  - Pause and gamepad disable park the session when the firmware supports it.
  - The worker accepts a PARKED status.
  - `KeychronOnboard_SetParkOnStop(false)` is called at the start of
    `AppShutdownNoThrow`.
  - `KeychronOnboard_ReleaseParked()` runs in `Backend_Shutdown`, so exit
    while paused returns the keyboard to ordinary mode.
- r6 firmware (no capability bits) keeps the previous behaviour.

Limitation: a crash while parked leaves an idle, neutral controller interface
until the 30 min park lease expires or the keyboard is replugged. Keys type
normally. While paused, Windows still lists the K4 controller, which stays
neutral.

## Validation

- Session and client portable tests were extended:
  - park, lease and reopen;
  - delta = 2 chunks;
  - resume with 0 chunks and no reconnect;
  - stale-base fallback;
  - r6 fallback;
  - exit release.
- Native backend checks PASS. `build_release.ps1` EXIT=0 (WinLibs first on
  PATH). Full catalog exit 0.
- EXE SHA256 `abc4cde1d6ba653f30838be698167ce352d2f7bce7d4e0987eb42636c5f0b89f`.
- Firmware: ARM build PASS (`.local/k4-r8-firmware-build.log`). The QMK bin is
  `.local/backups/k4-r8-qmk.bin`, SHA256
  `04634f345134222c1998c00e73fe201396bf682c68d652497377d587fb1aa335`.
  The r6 build-tree source is saved as
  `.local/backups/k4-build-tree-halljoy_onboard-r6.c`.
- FLASHED 2026-09-29 on owner request (r6 procedure):
  - The status was idle with caps 31 (r6). DFU was entered via A9/79, serial
    3381347A3035.
  - The pre-flash image was read twice identically:
    `.local/backups/k4-r8-before-flash.bin`, SHA256 31a28e04…d1f6.
    The application matched the r6 full image outside EEPROM.
  - The QMK suffix was stripped. The r8 raw image is 144124 bytes; its
    0x4000..0x7fff region was zero padding and was replaced by the preserved
    settings sector.
  - Full image `.local/backups/k4-r8-full.bin`: SHA256
    b702850a39700e1658ce9152bb31d587fd6419e2e9c0cca21ae5949e8c8a0b2a. The full
    256 KiB readback matched it before boot.
  - After boot: phase 0, native 0, caps 254.
- Hardware probe (production channel/client, neutral unbound profile,
  `.local/k4-r8-hardware-probe.log`):

  | Operation | Result |
  | --- | --- |
  | Full open | 2510 ms (re-enumeration) |
  | Block flag toggle updates | 23–32 ms each (previously ~1–2 s) |
  | Park | 6 ms; phase 3, native 1 |
  | Reopen from PARKED | 34 ms, no re-enumeration |
  | Exit release | 16 ms; afterwards phase 0, native 0 |

Backup before changes: `.local/backups/src-before-k4-r8-2026-09-29.tgz`.

## Update 2026-10-01

The `Backend_Shutdown` placement above was a defect: pause also runs that
function, so every pause released the park immediately and disabled parking for
the rest of the process. Since 2026-10-01:

- release runs only at exit, after the engine owner stopped;
- exit parks, then releases (no re-enumeration wait);
- the stop path detects a session that the firmware already parked.

Measured pause 170–290 ms, exit ~250 ms. See
[PERF_PROFILE_2026-10-01.md](PERF_PROFILE_2026-10-01.md).
