# SteelSeries family review — 2026-09-25

## Owner correction and original Apex Pro confirmation

The supplied Discord screenshot shows the tester saying "working now" before
sending message (3). That report is a successful-test log, not an unresolved
analog failure. Its early disconnected snapshot does not contradict later input.
The prior requirement for another diagnostic run is superseded. Original full-size
Apex Pro is Supported, with the existing firmware4.16.8 admission retained.
No claim that the agent physically tested it or measured precision is made.

Removed the original model from the yellow catalog, moved its README line to
Supported, updated detailed hardware/next patch notes and runtime wording.
Live Main!C695 changed Implemented; awaiting hardware testing -> Supported.
Fresh readback: exact brand/model match, dropdown unchanged, base/effective B:C
colors green (.65882355,.8666667,.70980394). Neighbor values unchanged by narrow
masks; borders/row heights untouched. Full yellow reconciliation PASS:251 models.
Evidence .local/apex-confirmed-sheet.json. Ordinary Release+6 executable gates
PASS: .local/apex-confirmed-build.log. EXE SHA256
 d399f327f881c729a6da719197e81f62bf40064538a8eeed8fb1c85f5e6ef511.
No publication.

## Official firmware inventory

Reused the official GG120 installer acquired in the original implementation.
Inventoried16 Apex Pro device entries (includes dongles and Bluetooth metadata,
not16 retail keyboards). Hashes and file sizes are recorded in
.local/research/steelseries-apex-20260925/family-inventory.json.
No vendor executable or firmware was run against a live device.

| Model/family | Official bundle | Result |
| --- | --- | --- |
| Original Apex Pro |272111120 /4.16.8|Enabled; user confirmed|
| Original Apex Pro TKL |272111124 /4.16.8|D7/DA/90 actual ARM handlers verified offline; integration candidate|
| Full-size Apex Pro Gen3 |272111168 /4.16.8|D7/DA/90 actual ARM handlers verified offline; integration candidate|
| Apex Pro Mini |272111134 /1.19.7|Different firmware family; not admitted|
| Apex Pro TKL2023 (internal2022) |272111144 /1.19.7|Different firmware family; not admitted|
| Apex Pro TKL Gen3 |272111170 /1.19.7|Different firmware family; not admitted|
| Apex Pro Mini Gen3 |272111176 /1.19.6|Different firmware family; not admitted|
| TKL Gen3 Cyber / Steel |272111200 /272111202 /1.20.00|Separate images inventoried; not admitted|
| Mini Wireless |272111140 dongle /142 keyboard /17896998 Bluetooth|Separate Nordic/STM/dongle images,3.24.1; Bluetooth metadata3.13.4|
| TKL Wireless2023 |272111152 dongle /154 keyboard|Separate Nordic/STM/dongle images,3.24.1|
| TKL Wireless Gen3 |272111172 dongle /174 keyboard|Separate Nordic/STM/dongle images,3.24.1|

Not finding the old dispatcher layout in the newer images is NOT proof that
live analog is absent. Their dispatch, key map, calibration and USB/radio routing
still need reconstruction. Do not send old commands to them speculatively.
No new neighboring model was enabled or colored yellow during this review.

## Two nearest firmware matches

Original TKL: SHA256
5fbd2253f0217692d0ad23c95c852dec0ce2df817cc37c39b10e1d5717e8d796.
USB descriptor PID1614 at image offset1aa20; base0800a000.
D7 read0800b744; DA read0800b3a8;90 read080119a0.
Sensor state20001348, HID map08023da8 exactly equals original256-byte ROM map.

Full-size Gen3: SHA256
4df0a1da0d9ca285cf1eb9f2a5f803b0c52a55a68abcddad7777159bc838280c.
USB descriptor PID1640 at offset1b3d4; base0800a000.
D7 read0801227c; DA read08011ee0;90 read0800c004.
Sensor state2000134c, HID map08024774 has69 HID entries mapping68 unique sensors:
extra OEM FB aliases Enter sensor41. Canonical sensor-to-HID map remains identical.
Do not expose the duplicate OEM alias as another physical analog key.

Adapted the existing hash-pinned verify_apex_pro_firmware.py offline fixture to
these reviewed addresses/hashes/maps and dispatch table offsets. Both passed:
five banks of raw+filtered ADC, calibration for every ROM entry, invalid banks/
counts, version4.16.8, sensor RAM unchanged and writes confined to output/stack.
This is execution of real read handlers with synthetic RAM, not full USB/device
emulation. Before runtime admission still verify HID caps, endpoint/dispatch/reply
plumbing and exact product naming; add PID-specific admission tests. Existing
backend still admits only1610. Handler compatibility alone is not a shipped path.

Official product context: https://steelseries.com/gaming-keyboards and
https://support.steelseries.com/hc/en-us/articles/33719007261709-What-type-of-switches-does-Apex-Pro-Gen-3-have
confirm model families and that only the OmniPoint section has adjustable sensors.
A blanket claim for all SteelSeries keyboards is inappropriate: the catalog also
contains conventional mechanical, hybrid and membrane switch models.


## Local integration and Release build — latest result

The two candidates above are now enabled: original Apex Pro TKL PID1614 and
full-size Apex Pro Gen3 PID1640. Exact VID/PID allowlist, FFC0:1 and65/65/643 caps,
firmware4.16.8 admission, five complete ADC banks, firmware calibration and
canonical68-sensor map feed the existing physical-analog/bindings/gamepad path.
RW handle identity is rechecked before sending any packet. Device telemetry uses
the exact model name. Original1610 is confirmed and excluded from yellow notices;
the two untested models carry the yellow notice. No new visual geometry inferred.

Expanded tools/verify_apex_pro_firmware.py to three hash-pinned profiles. All pass
real ARM-handler execution, full mapping (Gen3 extra OEM alias excluded from public
ownership), USB vendor descriptor, endpoint82/interface1, calibration, version,
invalid requests and unchanged sensor RAM. Original reset collision regression
still passes. Evidence: .local/apex-family-firmware-checks.log.
Targeted portable protocol/identity/caps/notice tests PASS, including rejection
of Mini/new TKL and unrelated SteelSeries IDs. Static native audit suite PASS.

### Newer firmware limits found

Mini1.19.7 is based at08009000. Its live USB dispatcher08012178 has a38-entry
command table08017b84. Replies contain command echo and status followed by payload;
endpoint82 still sends64 bytes. Thus legacy response parsing is incompatible.
Neither57 nor5A exists in its table. Five additional wired images (TKL2023,
TKL Gen3, Mini Gen3, TKL Gen3 Cyber and Steel) also lack these commands in their
recovered tables. Exact records/hashes are saved in
.local/research/steelseries-apex-20260925/modern-command-tables.json.

Mini command74 read0801095c returns a one-byte state, not key depth. Reads72/75/77
return one-byte configuration fields;71 returns a19-byte structure. Do not confuse
these with live sensor streams. Read32 (08010e9c) can call profile/state-changing
routines depending on its argument: a read-selector bit is not a safety guarantee.
The nine wireless MCU/dongle images did not match this table structure; absence
of a matching structure is inconclusive. Their radio/USB routing and independent
multi-key analog read path remain unresolved. No unsupported command sent to any
device. This does NOT establish that analog integration is impossible, only that
no safe complete path for these variants is currently implemented.

### Synchronization and validation

Live Sheet Main!C696/C700: Not investigated -> Implemented; awaiting hardware
testing. Seven other existing SteelSeries rows697..699/701..704 now accurately say
Research incomplete, retaining gray. Confirmed C695 remains Supported/green.
Fresh readback verified identities, all ten dropdowns unchanged, base/effective
colors, and full yellow catalog253 models PASS (.local/apex-family-sheet.json).
Narrow masks preserved borders, row heights and unrelated cells; no inserted rows,
notes or comments. README, hardware docs, runtime notices and patch notes agree.

Production-linked profile/layout checks and18 recovery scenarios PASS:
.local/apex-family-profile-checks.log. Layout coverage253 models:56 existing,
35 complete,6 partial,156 missing. Both new SteelSeries models use manual layout
selection; missing automatic geometry does not disable analog support.
Ordinary Release and six executable gates PASS (.local/apex-family-release-build.log).
Delivered build/bin/Release/x64/HallJoy.exe,9973760 bytes, SHA256
605a0b8b8f980335dcff16de4076905ec50887054b2ef3729ca788493816aa1e.
No forced diagnostic mode; no GitHub publication; local candidate version1.6.3.0.
