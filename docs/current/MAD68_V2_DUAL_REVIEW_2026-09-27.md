# MAD68 V2 Dual: current support review

## Tester result and retained low-quality implementation — 2026-09-27

Tester screenshot says analog works pretty well, but normal typing is unavailable
in game and Discord while HallJoy is active. This confirms the firmware's typing
limitation; it is not evidence of a suitable simultaneous analog/keyboard protocol.
Owner explicitly chooses to retain this implementation for people who accept
its limitations. Do not delete it or award Supported/yellow. This latest decision
supersedes the earlier suggestion to withdraw the trial. No flashing authorized.

Red banner now states the firmware lacks a suitable analog protocol, the available
method loses shallow presses/delays small changes and blocks normal typing.
"Best available" refers to the method found in the examined stock firmware;
no claim of exhaustive impossibility. Existing trial build scope is retained;
ordinary Release is not silently expanded. Manual Open log remains available.

No automatic log file due solely to this known-limitation banner: shared
ShouldAutoSaveSupportLog policy is used both by UI incident latch and background
writer, including before analog connection. Explicit logging, manual snapshots,
other support incidents and actual runtime failures retain their existing rules.
Real Windows writer regression PASS: limited banner connected/disconnected creates
no log; existing manual/warning/experimental/failure/recovery tests still pass.

Live Sheet Main!C503: Research incomplete -> Analog available; low quality.
Added that status to this cell's strict dropdown and existing red conditional
rule. B503:C503 base and effective red verified; neighboring values/colors intact.
No rows inserted, no notes/comments. Presentation palette recognizes the new
status. Fresh all-row status/color read matches all252 yellow runtime notices.
README supported lists remain unchanged; hardware document records this exception.

Rebuilt trial and all six executable gates PASS. SHA256
98a60fcc3dc1050966e64b3bc360b01a2a7eb651caebca7df9583c4935f7db1d.
Path remains build/bin/Mad68DualTrial/Release/x64/HallJoy-MAD68-V2-Dual-Trial.exe.
No GitHub publication; installed ordinary HallJoy unchanged. Separate startup
error5 screenshot was explained by owner: an existing HallJoy remained in tray.
No instance-guard changes were made.


## Limited stock simulation trial delivered — 2026-09-27

Latest owner decision supersedes the blanket stock-only discussion: first offer
this tester the existing imperfect analog, without flashing. If unacceptable,
the owner will ask the tester about custom firmware. No flashing authorized now.
Do not award Supported or the normal yellow fully-implemented status merely
because this private trial produces gamepad input. Evaluate tester feedback.

Implemented an isolated compile-time HallJoyMad68DualTrial variant, exact
28E9:3265 only, with mandatory FF87:20 report06 IN/OUT64 and FF88:21 report07
IN3 topology. Multiple matching keyboards are refused in this first trial.
Normal Release does not compile/register this backend. Ordinary HallJoy input,
bindings, curves and ViGEm remain enabled in the trial; no synthetic keyboard
reinjection or added analog smoothing. Installed ordinary app was not replaced
or restarted; this separate output is intentional tester isolation.

The session reads device info12 and default physical map16, requires a usable
map including WASD, then enables36:01. No calibration37, settings writes, reset
or bootloader commands are sent. Mode36:00 cleanup is attempted on ordinary
exit, failed enter ACK, read failure and C++ unwinding. Stop waits for bounded
I/O and does not cancel its own exit command. Late enter acknowledgements cannot
be mistaken for exit acknowledgements: the echoed enable byte must match.
Abrupt process termination/power or USB loss cannot guarantee cleanup; reconnect
USB if normal typing does not return. One active attempt per process; restart
HallJoy after a failed attempt or unplug/replug. No automatic repeated probes.

Low-first six-bit report pairs become raw0..3250, then ordinary milli0..1000.
Decoder rejects malformed bytes, raw overflow, different-key/long-gap pairings.
No sequence marker exists in this firmware; undetectable fragment loss cannot
be completely repaired. Silent held keys retain their last value; silence alone
is not treated as a release. On disconnect/session failure all published values
are cleared. Firmware shallow cutoff (~6–14% for the specifically replayed
branches, not a universal physical travel measurement) and small-change delay
remain unchanged. Normal key processing is diverted in this mode.

A red limited-analog banner explains shallow/deferred values and possible typing
loss, with Open log. Structural session/mapping/ACK/first-pair/failure events are
recorded via SupportLog; no per-key input values or forced continuous logging.
Existing missing/experimental banner snapshot behavior is retained.

Validation: portable parser tests cover actual ARM-replayed packets, releases,
malformed/interrupted pairs and all3251 valid raw values. Trial vs normal notice
classification tested. Production session compiled with fake synchronous HID
I/O passes normal values/release, rejected enter ACK, disconnect, C++ exception,
invalid mapping and stop during mapping; exit cleanup and stale ACK exclusion
verified. These tests open no keyboard. Expanded ARM audit passes all three
pinned firmwares, including echoed mode byte. Release trial and all six linked
image checks PASS. Existing ViGEm PDB warning is debug-symbol-only.

Executable: build/bin/Mad68DualTrial/Release/x64/HallJoy-MAD68-V2-Dual-Trial.exe
SHA25664d0b87d7397bd71d24a25789431b83867c9a5e7c362a1db3b90dd860ef3baaf.
Builder: tools/build_mad68_dual_trial.ps1. Tests:
- src/HallJoyProject/tests/mad68_dual_trial_protocol_test.cpp
- src/HallJoyProject/tests/mad68_dual_trial_session_test.cpp
Evidence/logs: .local/mad68-v2-review/. Backup before shared-source edits:
.local/backups/mad68-dual-trial-1790503605.zip.

Support synchronization: exact Dual model was absent from live Sheet. Inserted
Main!A503:C503 as MADLIONS / MAD 68 V2 Dual / Research incomplete (gray), preserving
all existing model/status values, column widths, validation and brand block.
Fresh full native readback: strict dropdown, gray base/effective colors;
structure/presentation audit PASS148 blocks; yellow notice catalog PASS252.
Packed before/after native backups and insertion plan retained under evidence.
README supported/experimental lists deliberately unchanged; hardware document
records the private trial separately. No GitHub publication, no physical tester
result and no acceptance of this restricted protocol as full support.


## Stock-only investigation continued — 2026-09-27

Owner explicitly requires support **without reflashing**. Custom firmware is
not an accepted next step. This supersedes the earlier suggested firmware path.
No HID commands were sent to a physical keyboard during this investigation.

The actual dispatch bounds/jump table was replayed for every byte opcode on
1.06, 1.07 and 1.09. Each accepts 47 commands; 209 reach the default rejection.
This proves dispatch coverage, not exhaustive execution of every payload/state.
Accepted commands: 10–1E, 20–2A, 30–3B, 80–86, B0–B1 (hex).
The audit stops before command bodies for this enumeration: calibration,
settings writes, reset and bootloader entry are never executed as probes.

Additional static findings on 1.09:

- 81 sets RGB-test parameters. Its consumer loops over the LED matrix and
  supplies RGB constants; it is not a sensor read.
- 82 enables a factory binary-event branch. The four-entry ROM table contains
  only one non-FF record: key index3F, event value01, type40. ARM replay on all
  three versions sends 070040 for zero and 070140 for every tested nonzero
  state (1,127,255); matching events skip ordinary key processing. Unmatched
  key0 continues normally. These are binary states, not depth samples.
- 83/84 are calibration modes; exit paths call the persistent calibration
  writer. 85 reads stored calibration from flash, 86 checks stored values.
  They are unsuitable as a read-only gameplay telemetry subscription.
- 14/17/19/26/29/33/38/3A read settings/cached settings. 16/21/23/24/28 read
  default maps or mapping RAM/ROM. No live sensor source was established.
  The common cache starts at20009DD8, above observed live depth buffers;
  its unsigned offset is not a supported arbitrary sensor-memory read.
- Interrupt OUT report06 reaches the enumerated dispatcher; report02 handles
  keyboard LEDs. The reviewed HID class handler provides no GET_REPORT data.
  Twelve actual GET_REPORT branches (interfaces0/1, types Input/Output/Feature,
  IDs6/7) replayed on1.09 leave response pointer/length unchanged. This is
  handler-level evidence, not a claim about every USB framework request path.
- B1 is bootloader/reset-related and must never be tried as an analog probe.

Official POST device/historyVersion for pid12901 (page0,limit10,type1) returned
exactly three entries: 1.09,1.07,1.06. Saved history-12901.json records URLs and
release notes. This is the current API inventory, not all firmware ever made.
Downloaded API1.07 application is byte-identical to the first86472 bytes of the
previously decoded updater container; the remaining25 bytes are outside that
application. API1.06 is86472 bytes, SHA256
8fb6cec0467c66cb2d02a831db5f0dd9b92ee5ae44ff57efce4c55a51ed37057.
Local API artifacts are named MK655-Dual-V1006-api.bin and V1007-api.bin.
The awkward filename is an acquisition label, not firmware version10.06.
API1.07 SHA2566057a33f1ffb5b670c093dd21f9ecec07c8822a9d4a811a680596931d0c81dcb.

Expanded tools/mad68_v2_dual_firmware_audit.py PASS: three pinned versions,
768 opcode routes, simulation thresholds/delay/report checks, binary factory
branch checks, and12 HID GET_REPORT cases. Saved arm-replay-expanded.json.
1.06 reproduces the same command36 small-value threshold and31 additional
per-key passes for the tested stable small change as1.07/1.09. No timing in
milliseconds or complete hardware emulation is claimed.

Conclusion: no clean stock analog transport found after this deeper pass.
Do not advertise simulation as full support or hide its loss/delay with host
smoothing. This is not proof that every hidden mechanism is impossible; an
undocumented vendor interface or different installed firmware remains unknown.
A productive external lead is vendor confirmation of read-only per-key depth
outside simulation/calibration (or a precise supported alternative command).
No message sent to the vendor, no flashing, production integration, build,
support-status change, Sheet change or publication. Existing app stays intact.


## Firmware acquisition and ARM replay (2026-09-27)

Exact identity established: the FGG V2 client maps 10473:12901 (28E9:3265)
to MK655 dual-light hardware. Its download page explicitly labels the package
**MAD 68 V2 Dual**. Both downloaded applications contain USB descriptor
28E9:3265. This supersedes the earlier candidate-only identification and the
DuckBread373B lead; those are different revisions.

Sources and retained evidence under `.local/mad68-v2-review/`:

- FGG V2 frontend: https://v2-fggtest.i-game.tech/ , assets/index-BiIww4Tf.js.
  SHA256 6ac98c73032796fbe2ae8823556b30d1783eea4edc7d223098a24d3d4fb887e4.
  Main v2-hub.fgg.com.cn failed local TLS acquisition; the V2 test frontend
  explicitly links that production frontend and exposes the exact model.
- Official v1.07 DoubleLight updater linked in the frontend, downloaded without
  executing it. Exact URL retained in firmware-source-url.txt. SHA256
  4a11126d207473c379a08b34a4ebe0543e25a2b41c599fca2b1b2be770bb3f74.
- POST https://api.i-game.tech/api/device/upgrade with
  {"pid":"12901","version":"0","type":1} returns v1.09 and model label
  US_MAD 68 V2 DUAL_XUEYAO_1.2MM. Response/URL retained in upgrade-12901.json.
  Downloaded raw application SHA256
  066655ce6efc9bcca590fe7423a083b6b92ec8f42854d18e412c6afc697267cf.
  API md5 field 5376e3e0d103ff57a842e575a74aadbe does NOT equal downloaded MD5
  c56dde31de287756262340834fbca389. Repeated download matched the latter;
  API hash semantics/metadata remain unresolved. This artifact is for analysis,
  not an approved flash package. No updater executed and no device flashed.

v1.07 PE overlay starts391E00, target name occupies128 bytes. Payload bytes
starting391E80 XOR with (AA+index)&FF recover code/vector data. The decoded
container includes trailing package metadata; do not call its entire length an
independently verified firmware payload size. Correct flash load base08008000,
reset080081B5 resolves to startup at file01B4 (not base08000000). Thumb-2 code.
Both retained disassemblies were regenerated with the correct base.

Transport is the V2 family: control usageFF87:0020, report06,63 payload bytes;
stream usageFF88:0021, report07,2 payload bytes. Firmware descriptor addresses:
v1.07 080185C6/080185E1; v1.09 080187D6/080187F1. This resembles Titan transport
but compatibility conclusions below come from these exact MK655 binaries.

Command36 handler v1.07=0800A62A, v1.09=0800A65E toggles bit1 at
20006A6A /20006C7A. Its reviewed body and called clearing helper write volatile
RAM and clear key/work state. The replay stops at the common response tail;
it does not prove the behavior of every firmware command or physical device.
Command37 is a separate calibration path and is not used.

`tools/mad68_v2_dual_firmware_audit.py` pins both hashes and replays actual ARM
instructions with Unicorn. Result: arm-replay.json, PASS for both versions.
No connected HID access. Synthetic RAM and a mocked USB-submit function are
explicit boundaries; this is not a full MCU/peripheral emulator or timing test.

Confirmed limitations of the command36 simulation branch:

- Simulation bit1 diverts the per-key routine away from its ordinary processing
  branch. It is not merely an extra read-only telemetry subscription. Static
  branch evidence does not replace a physical test of all normal keyboard keys.
- Before publishing, depth is clamped to3250 and small values are zeroed.
  Replayed key index0 threshold200 or400; index15 threshold260 or460, depending
  on the existing firmware setting at settings+3D. No millimetre conversion or
  exact user setting claimed from these branch tests alone.
- A depth1500 sends actual packets07005C,070057; zero sends070040,070040.
  Two successive six-bit fragments reconstruct the12-bit depth.
- Stable small step1500->1510 sends nothing on its first pass, then sends
  070066,070057 only on pass32 (31 additional calls). The branch uses a30-count
  stability counter for small changes. Do NOT translate this into milliseconds
  without the physical scan cadence. A large release to0 publishes immediately
  in this synthetic path.
- These findings are identical on1.07 and1.09, including generated packets.

Other obvious frontend leads reviewed: command38 reads travel settings, not
live depth; command86 reads persisted calibration data and returns a test flag;
command3C lands in the unsupported-command response on1.09. No clean alternate
live-read path has been established. This is not proof that all possible hidden
commands have been exhausted.

Conclusion: firmware successfully acquired and protocol behavior established.
Do not add the PID blindly to legacy MADLIONS or present command36 as unrestricted
support. A usable integration still needs a verified alternative live-read path
or a deliberately changed firmware transport that preserves normal processing.
No HallJoy runtime/backend, support status, README support list or Sheet change;
no publication. Existing installed banner-log build is unchanged.

## Update: received log and banner log access

The owner supplied Downloads/message (4).txt and identified the missing-log
confusion: Explorer hides extensions, so HallJoy.log appeared as a text document
named HallJoy. Recording worked; continuous logging was enabled during the session.
The inventory has no 373B device. 28E9:3265 is a candidate for the user's keyboard,
not yet a proven model identity. Do not transfer DuckBread V2 conclusions below
to this Dual revision. No support status changed. Late analog_error=-2000 is an
SDK UnInitialized result, not proof of a USB transport failure.

Owner requested Open log in red and yellow banners. Implemented one shared
button, including the communication warning. It requests a fresh snapshot and
opens the completed primary HallJoy.log in system Notepad; a UI timer waits
without blocking input. Failed writes/opening are reported instead of silently
opening a stale file. Continuous logging preference remains unchanged.

Explicit owner correction: communication instability alone must NOT force log
recording. Automatic incident writing covers missing/unverified support when the
communication warning is absent. Manual Open log still saves on demand. Existing
engine/host failure logging is unchanged. No extra HID probes or input-value logs.

Validation: real Windows writer regression PASS (warning alone creates no file,
manual snapshot completes without enabling logging, experimental banner writes,
mirror remains opt-in, failure/recovery/privacy checks). Ordinary Release and all
six candidate gates PASS. Candidate SHA256
63a35ca8d98bf9d2a034bc300b9c3c0389eb674b7d1efe89d91c57007305b436.
Installed successfully via RunAs standard publisher after confirmed elevation
mismatch; target hash matches candidate and previously running app restored
(WasRunning=true, Normal). See BUILD_LIFECYCLE_2026-09-27.md.
Logs: .local/banner-log-test.log and .local/banner-log-release.log.
No publication. Owner evaluates visuals.

## Initial review (superseded where corrected above)

2026-09-27. Owner reports a user's red No supported analogue keyboard detected
banner. User also cannot find a log after enabling logging. No device log yet.

Current support covers original MAD68HE, MAD68R and MAD 68 Pro R identities,
not the official V2 entries. Live https://hub.fgg.com.cn/ still references
index-CfSMCN5O.js, byte-identical to the saved official catalog (SHA256
c2855cf201713fe102b110f96423f70310c8e7623afc0a943119a66d6dd27418).

Official DuckBread V2 entries: vendor373B, PID1123 (V2),1124 (V2 RGB),1125
(V2 Ultra), usageFF60:61, matrix5x15. None of these PIDs is enabled in current
UAP recognition; native Pro R remains restricted to373B:1109. The word Dual
is absent from the decoded catalog, so the user's precise identity is pending.

DuckBread V2 reads ADC/travel status in groups of four. Existing UAP MADLIONS
uses four-record02/96/1C polling. This is a compatibility lead, not proof of
live analog outside calibration on this exact revision. No firmware download,
emulator run, implementation or status promotion claimed. No Sheet update needed.
Evidence: .local/mad68-v2-review/ (official entry/catalog and static decoding).

Logging source review, including the publication mirror: the missing-keyboard
incident writes AppPaths_DataRoot/HallJoy.log even without continuous logging.
Enable logging additionally mirrors to the executable directory. The ordinary
writer checks every second, retries failures at5s intervals and exposes a
Windows error in Global settings. First user step: Open HallJoy folder, which
opens the primary data folder. Default path is %LOCALAPPDATA%/HallJoy. Absence
there is unresolved, not evidence that the user overlooked it.

Profiles hidden at owner request: kProfilesPageEnabled=false prevents tab/page
creation and its automatic foreground service; implementation/data retained.
Release plus six candidate gates passed (.local/profiles-hidden-release.log).
EXE SHA25697af684f80837d5effd975980bc9bce37b8fa9e9526bc633a63874cb5bd1d2d7.
Previously running HallJoy restored automatically. No publication.

## Custom firmware feasibility question (not authorization)

Owner asked how difficult custom flashing would be and whether protections exist.
This question does not revoke the stock-only requirement. Reviewed official V2
frontend exposes B1 bootloader entry and B2–B7 status/info/start/data/complete/end
update operations. Application binary is readable, and updater XOR packaging is
reversible; neither proves unsigned modifications will be accepted by bootloader.
No bootloader image/security policy or recovery mechanism has been established.
Do not claim absence of signatures, write protection, or guaranteed USB recovery.
A targeted stock-binary telemetry patch is a plausible research direction, not a
ready flashable build; preserving scan, calibration, typing and USB scheduling
requires validation. No updater executed, no device altered.

## Expanded behavior review — 2026-09-27

See [pipeline review](../development/FIRMWARE_BEHAVIOR_REVIEW.md). All144 indices,
both settings and18 boundary depths replayed on all3 versions:15552 samples PASS.
Additional restriction: indices13,15,30,42,44,55,61 snap to3250 strictly above3170
(setting0) or3070(setting1). Physical key identities not established here.
Command36 remains best reviewed limited source; unknown lifecycle/USB properties
are explicit, not accepted. This is not a public support reclassification.
No further diagnostic build or hardware action performed.

## Mode/USB fault replay — 2026-09-27

The new targeted suite replays21 scenarios across1.06/1.07/1.09. Model: command36
and post-ADC branch joined through an explicit RAM-mode-to-register bridge;
synthetic ready byte and submit return. Busy polling reaches the instruction bound
and resumes after modeled readiness restoration. Dropping either release fragment
leaves no retry of that fragment in40 later per-key passes. A separate global USB
retry path or hardware behavior has not been ruled out. Disable reaches ordinary
processing entry, then reenable resumes analog; full typing delivery remains
unknown. No support reclassification. Hash-pinned evidence:
`.local/firmware-behavior/mad68-usb-lifecycle-verified-20260927/`.
See the firmware behavior runbook for commands and model boundaries.
