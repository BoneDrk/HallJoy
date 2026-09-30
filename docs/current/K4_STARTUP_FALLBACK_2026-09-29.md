# K4 HE onboard: occasional "unstable" warning and fallback at startup — 2026-09-29

Owner report: sometimes at startup HallJoy reports the K4 HE connection as
unstable and uses a worse fallback protocol.

## Evidence (support log `%LOCALAPPDATA%\HallJoy\HallJoy.log`, 22:08 session)

- `keychron-k4-onboard-hjo1 ... observation=not_present` for the whole session
  while 3434:0E40 was present in the HID inventory; `sdk_sources=1
  native_sources=0`: the K4 ran through UAP (analog report) instead of onboard.
- `uap.child_exit value=0xE0485632` (`kHostExitProviderPlaneResize`, a planned
  host restart) 16 ms before `keyboard.communication_warning value=1`.
- That session was HallJoy 1.6.5.0 (not the local 1.6.6 build).

## Causes

1. Onboard routing is decided once per engine generation (`Prepare`) before
   UAP starts. If the K4 is not ready at that instant — after a previous
   session it leaves native mode and re-enumerates USB; its vendor interface can
   be briefly missing or still held — or one status request is silently dropped
   by busy firmware, the claim fails and UAP owns the K4 for the whole session.
   The failure was not logged.
2. The UI communication observer treated every UAP host restart count increase
   as a fault, including the planned provider-plane resize restart that occurs
   during normal startup. This produced a false "unstable" warning.

## Fix

- `Prepare`: up to 3 attempts of the status request; bounded retry (100 ms
  steps, ≤ 3 s) only while a K4 USB device node is present
  (`K4UsbDevicePresent`, metadata only). No delay without a K4.
- Support log: `k4.onboard_prepare_failed value=<stage> data=<attempts>`
  (1 no device, 2 ambiguous, 3 open, 4 status, 5 capability, 6 routing) and
  `k4.onboard_prepare_retried value=<attempts>`.
- Analog host counts unplanned restarts separately (crash/failed launch only);
  the communication observer uses that counter. Planned resize/device-refresh
  restarts no longer raise the warning. Real host faults still do.

Validation: native backend checks (compiler) PASS; `build_release.ps1` EXIT=0;
`--halljoy-require-full-catalog` exit 0; installed `build/bin/Release/x64/HallJoy.exe`
SHA256 `863F06AC08932436839166963C763302F7EA15BF5AC82B4D1E2207CFA9435B85`.

Not changed: a K4 that becomes ready only after the 3 s window still uses UAP
until Pause/Resume (no automatic late takeover). If the new log event appears,
its stage identifies the remaining cause. No hardware verification by the agent.
