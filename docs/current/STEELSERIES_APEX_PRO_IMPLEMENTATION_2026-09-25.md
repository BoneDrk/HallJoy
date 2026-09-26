> Superseded tester interpretation: the user confirmed working input before message (3). No repeat test is required. Original Apex Pro is now Supported. See [family review](STEELSERIES_FAMILY_REVIEW_2026-09-25.md).

# SteelSeries Apex Pro native analog — local 2026-09-25

Scope: original full-size Apex Pro1038:1610 from Downloads/message (2).txt.
No physical-device test or new GitHub publication. Other generations are not
aliases. Exact enabled firmware is4.16.8; unknown versions receive only the
read-only version query, then a diagnostic explaining the admission hold.

## Primary evidence

Official https://steelseries.com/gg/downloads/gg/latest/windows redirected to
https://engine.steelseriescdn.com/SteelSeriesGG120.0.0Setup.exe . Downloaded
432696832 bytes with curl; selected files extracted with7-Zip, never executed.
Acquisition manifest and firmware remain in .local/research/steelseries-apex-20260925/.
Image apps/engine/firmware/272111120/keyboard-app.bin is118784 bytes, SHA256
750e0b987eac9c6f9596db882dc4ce423e63994b8082b957571ddaeb69ea5bbc.
version.json calls the device apex_pro and version4.16.8/4.16.8.
Image base0800A000, reset0801ED59, device descriptor08024A44 gives1038:1610.
Vendor HID report descriptor08024BF8: usageFFC0:1,64-byte input/output,
642-byte feature; Windows lengths include the report-ID byte:65/65/643.
No feature reports are sent. Output uses HID WriteFile/control SET_REPORT;
interrupt input endpoint82 sends64 bytes with no opcode/bank echo.

Dispatcher08013C46 masks command with7F, indexes64 records at08022314,
then bit7 selects read pointer (+4) versus write pointer (+8).
Main-loop reply0801596C sends the zero-padded64-byte result to endpoint82.
Input callback080112DC copies request into20003064; no Report ID on MCU side.

-90 00: keyboard firmware version, read handler0801199C returns ASCII4.16.8.
-D7 bank1..5: read handler0800B7C8. First28 bytes are14 raw ADC halfwords;
 next28 bytes are14 filtered ADC halfwords; remaining8 bytes zero padding.
-DA count (HID,0,0,0,0)*count: read handler0800B42C/0800B3B0.
 Result repeated(HID,max uint16LE,min uint16LE), max12 entries per exchange.
 Invalid regional/unpopulated calibration retains reversed extrema; skip it.
 Explicitly never send57/5A writes, calibration mode, reset, profile or flash commands.

Sensor RAM20001348: raw70 halfwords, max+8C, min+118, adaptive baseline+1A4,
 filtered halfword at+24C+32*sensor. ADC/DMA scan08010E50 writes the raw array;
 main scan0800CFB4 processes70 channels and updates filtered data. Its normalized
 response increases with(filtered-baseline)/(max-baseline); HallJoy uses returned
 calibration min/max, not unexposed adaptive baseline. Thus output is normalized
 sensor response, not a claim of linear millimetres. Range/baseline drift and
 noise at rest need physical confirmation. No additional temporal filter or
 digital-switch threshold is applied by HallJoy.

ROM HID->sensor table08023DD4 has68 valid positions (0..67), two spare slots.
W/A/S/D map to16/29/30/31. OEM codeF0 is excluded from public HID ownership;
regional positions require valid calibration. Ordinary non-OmniPoint switches
outside this section have no analog claim. Exact visual geometry is deferred;
manual layout selection remains available.

## Implementation

steelseries_apex_backend.cpp and steelseries_apex_protocol.h register protocol25.
Exact VID/PID and vendor caps only; exclusive collection, flushed initial queue,
one outstanding request,100ms exchange deadline, cancellation/drain on stop.
Any failed or malformed response ends the session before another bank is sent;
no retry that could mistake a late untagged response for the next bank.
Only validated complete five-bank frames publish. Values expire after100ms;
errors/disconnect/stop immediately clear publication. Dynamic firmware extrema
refresh every2s; changing populated set requires readmission. W/alphanumeric
calibration must be valid before connection. Curves/bindings/ViGEm use ordinary
physical-analog publication. No forced logging; when logging is enabled, record
firmware, per-key extrema, stage/opcode/bank/length/error and first ADC snapshot.

Potential contention is reported as a hypothesis, not proof GG is running.
Owner confirmed the cyclic USB restart occurs when HallJoy starts. The earlier
unknown-cause triage is superseded by the cross-protocol reset finding below. Firmware4.16.8 restriction is deliberate
and should only expand after reviewing the matching image.

## Validation and synchronization

verify_apex_pro_firmware.py executes the pinned official ARM read routines under
Unicorn with synthetic sensor RAM: five raw/filtered banks,68 calibration pairs,
invalid bank/count and version. Sensor RAM is unchanged; memory-write hook permits
only output/length/stack. This is handler emulation, not USB/hardware emulation.
Portable production-header tests check read-only request allowlist, exact mapping,
partial/reversed/calibration replies, ADC bounds/padding and monotonic scaling.
Build and Sheet checks are recorded below. The initial full native suite passed;
final rerun after the reset fix PASS: all static and portable C++ tests,
.local/apex-native-final.log. Final whitespace-only header warning cleanup
passed targeted production-header test and rebuilt ordinary Release+6 gates
(.local/apex-delivery-build.log). No new compiler warnings remain in that header.
Production-linked profile/layout tests and18 recovery scenarios passed on the
updated runtime: .local/apex-profile-final.log. Yellow layout coverage252models:
97 with existing coverage (some partial),155 missing; Apex uses manual selection.


## Cross-protocol reset found and fixed

Owner clarification: the keyboard's cyclic restart starts with HallJoy.
Old SparkUsageSupportScore admitted any FFxx:1 interface with sufficient report
length, including Apex Pro FFC0:1,65/65 bytes. It then sent SparkQueryDeviceInfo
payload01 02 before establishing vendor/product protocol identity.

In the official Apex image, command01's read AND write table entries point to
08011219. Request byte1=02 calls0801A5A0 withr0=0, which writes05FA0004 to
SCB AIRCR E000ED0C (SYSRESETREQ), then spins awaiting reset.
Extended verify_apex_pro_firmware.py reproduces this exact register write from
01 02 under Unicorn. Only the delay routine is bypassed; reset handler executes.
No live device received this packet. This proves a deterministic reset mechanism
in reviewed firmware matching the observed symptom; tester packet capture/version
was not available to independently prove every event in the original log.

Fix: SparkLink now uses exact known1CA6 product IDs, including confirmed0529
MG75 Max and all experimental revisions, plusFFB0:1 and live semantic proof.
Generic FFxx fallback is removed. Cached attributes are read with access0 before
any read/write handle; identity is rechecked before commands. No other device is
sent SparkLink info/reset-colliding commands. Previously speculative generic
probing is deliberately not a supported-model claim.

Also remove unnecessary UAP read/write enumeration of parsed USB vendors absent
from Soup's8 supported-vendor cases. Unknown/nonstandard paths remain compatible.
The same shared policy filters supervisor topology snapshots from Configuration
Manager (no HID open). WM_DEVICECHANGE only replaces UAP when its possible-source
path set changes; virtual gamepads/SteelSeries/unrelated vendor churn no longer
trigger unconditional child replacement. Snapshot taken before child creation.
The cause of the Apex reset is the Spark command collision, not proof that UAP
opening a handle alone reset it. Both paths now avoid irrelevant work.

UAP rebuilt from pinned dependencies, private ABI runtime gate PASS with local
custom Keychron (1device,134 samples, equivalent ABI/provider views). Embedded
abiv1 SHA2568c8f02670fbc49282afbfe9992786021069e562ad353822cacb606aba864d68e.
Ordinary release build and6 executable gates PASS; final EXE SHA256
ce57a133fe5aa2d4e9239d04ac3733be04b847472d4411cef0e1b203951fd955.
No forced log or release publication; version remains1.6.3.0 local candidate.

Live Sheet: original Apex Pro was missing. Inserted Main!A695:C695 before Apex Pro
Gen3, using allowed dropdown value Implemented; awaiting hardware testing.
Fresh native readback verified647models/148blocks,252yellow runtime notices,
base/effective colors, borders/heights, validation, no notes, unchanged old model
values and validation. Other9 SteelSeries rows remain Not investigated.
Snapshots .local/apex-sheet-native-before.json and apex-sheet-native-after.json;
planner12requests; structure/presentation and full yellow reconciliation PASS.
README, detailed hardware list, runtime banner and next patch notes agree.


## Message (3): ordinary Release diagnostics correction

Tester report message (3).txt SHA256
b7b7701fb1894b7d1d28b731df9f4dbae3367909a53066b12c303a240ab856e3,
5664 bytes, covers 12.641 seconds. Both HID inventories retain Apex interfaces
00/01/02/03/04. Four startup device-change events do not continue as a reset
storm; UAP starts once, reports zero restarts and exits cleanly. This short run
shows no recurrence, not indefinite stability. Last recorded snapshot around
one second has analogue_connected=0; no later snapshot proves its final state.
Version 1.6.3.0 alone cannot identify the exact tester EXE.

Missing Apex evidence was our diagnostics defect: previous firmware/range details
used DebugLog_Write, compiled out of ordinary Release. Enabling ordinary logging
does not enable those calls. Do not ask for another existing full log as a remedy.
No firmware rejection, calibration failure or analog cause can be inferred here.

Added ordinary SupportLog_Event stages: backend_start (revision2), unique HID
caps and collection admission, open/flush/query, firmware, read/write failures,
calibration validation/ready, depth rejection, first analog connection and end.
Caps value packs page/input/output/feature in four 16-bit fields; error is usage.
Firmware value packs first eight ASCII bytes little-endian. I/O failure value
packs command at bit48, argument at bit32 and completed bytes; error is Win32.
Calibration rejection value/error are range start/count, not pressed-key data.
No sensor/key values, forced logging or successful per-frame log writes added.
Existing optional debug-only sensor details remain separate.

Ordinary build and six executable gates PASS (.local/apex-message3-build.log).
Verified all 15 direct Apex structural event literals exist in the delivered EXE.
EXE SHA256 a1abfbe55647662e58abadf68f3ffeb93e56ef23aaaa3e1015e2bb85f34b4414,
9973760 bytes, build/bin/Release/x64/HallJoy.exe. No hardware reproduction here.
Fresh tester run/log required to identify the remaining analog problem.
Experimental status unchanged; no Sheet status change or GitHub publication.
