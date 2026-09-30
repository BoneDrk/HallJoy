# Pause and Resume shortcuts — 2026-09-27

Owner requested application-wide pause/resume bindings independent of Block Bound
Keys, accepting pure analog input, with either a toggle or separate actions.
Owner explicitly requires FULL keyboard release on Pause. No analog provider or
Mix87 flag is retained on Pause. Ordinary keyboard input is the resume path after
native cleanup. No known supported keyboard was established as permanently analog-
only after cleanup; the earlier hypothetical concern was not device evidence.
Failure to restore ordinary typing must be investigated as a cleanup defect.

## Implementation

Global Settings contains a separate-keys checkbox and assign/clear controls.
Default unassigned; toggle and separate bindings are remembered independently.
Pause/Resume use physical HID key identity (single keys, not modifier chords).
Capture accepts digital or analog presses, Esc cancels; hide/deactivation cancels.
Capture starts after analog release; assigning a held key cannot issue a command.
Separate Pause/Resume cannot share the same key. Window-level packed settings
are saved atomically, validated, restored on failure and isolated from profiles.

An existing dedicated keyboard hook observes noninjected input before the
Block Bound Keys decision. The existing realtime reader observes configured keys
through its raw cache, even if absent from bindings/visible layout. No new worker
or background UI timer is introduced. Analog threshold120/1000 with release60
provides hysteresis independent of output curves/deadzones. Digital/analog
signals share press ownership; auto-repeat does not issue repeated commands.
The UI queue carries one-way Pause or Resume requests stamped with the engine
command generation; obsolete requests are ignored after a state/generation change,
during capture or shutdown. A completed Pause discards stale analog samples so
the first ordinary Resume press is not swallowed if no digital key-up existed. Normal pause cleanup remains intact.
Saved bindings for the inactive shortcut mode do not cause analog reads.
K4 onboard telemetry stays available while hidden only when shortcuts need it;
Pause still stops its worker and releases its session.

## Validation

- Pure model: analog/digital deduplication, hold across pause, separate actions,
  inactive action, capture suppression, hysteresis and corrupt settings PASS.
- Real Win32 message-only window adapter: analog-only pause, ordinary resume,
  repeated events, capture and disabled/no extra reads PASS.
- Production-linked private-desktop Global Settings controls: mode visibility,
  capture/assignment, clear/cancel PASS; no visual assessment claimed.
- Production settings transaction tests: roundtrip and profile isolation PASS;
  existing layout/profile checks and18 startup recovery cases PASS.
- Full static audits PASS. Standard Release installation result recorded below.

Evidence: .local/pause-hotkeys-profile-final.log, pause-hotkeys-static.log,
pause-hotkeys-windows.log and pause-hotkeys-release.log. Backup:
.local/backups/pause-hotkeys-1790529178104480800.zip.
No GitHub publication or support-status changes; no Sheet edits required.

## Delivered locally

Ordinary Release build, mandatory diagnostic gate, six executable checks and
embedded ABI1/legal-resource byte checks PASS. Installed normal
build/bin/Release/x64/HallJoy.exe; SHA256:9c1577f4f0cc5838d5c1c3093b3098af6828bd7572945931346993ae8c4a2f2b.
Final evidence: .local/pause-hotkeys-release-verified.log,
pause-hotkeys-final-tests.log, pause-hotkeys-profile-verified.log and
pause-hotkeys-static-final.log. No GitHub upload.
