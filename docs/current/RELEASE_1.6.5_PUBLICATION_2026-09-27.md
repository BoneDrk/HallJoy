# HallJoy 1.6.5 publication — 2026-09-27

Owner explicitly authorized publication. Title: v1.6.5: MCHOSE support and tray controls.
Fresh publication checkout starts at cleaned GitHub main 4af3fcf4f3f49752500926cf670ea46a012102f5.
No historical tag rewrite or replacement of prior release assets is allowed.

## Scope

Mix87 III stock1.22 Supported after tester confirmation: automatic analog flag
ON at start/resume, OFF at pause/exit. No calibration commands. Persistent flag
and shallow/event-stream limitations remain documented; an enabled flag after
power loss has no demonstrated harmful effect. MAD68 V2 Dual remains red/limited
in its separate compile-time trial, not promoted to ordinary release support.
Tray preferences/actions/pause icon, hidden UI scheduling, editor panning and
performance, support-log access and schema2 diagnostics are included. Profiles
remain hidden; private K4 work is intentionally absent from public patch notes.
Dependency provenance/ABI1-only packaging and embedded legal viewer included.

## Validation

Ordinary local Release1.6.5.0 passed diagnostic contract gates, six executable
checks and exact embedded legal/ABI1 bytes; standard publisher installed the EXE.
Clean publication checkout full static checks and publication input policy PASS.
Public portable testing found one stale dependency on a removed private capture.
The RongYuan test now always checks 8,388,608 synthetic wire cases and explicitly
reports private capture replay NOT_RUN when absent. Local private replay passed
all4,898 captured frames. This changes tests only, not the released EXE.
Earlier tests passed in the initial run; the runner resumed at the corrected
RongYuan test and covers all remaining tests without repeating passed binaries.
A second runner issue was fixed: the header-only MAD68 trial parser test needed
explicit registration rather than automatic .cpp lookup. Its test and the
remaining automatically discovered protocols were resumed in release165-public-final.log.
Logs: .local/release165-public-portable.log, release165-public-resumed.log and
release165-ry-private.log.
No visual evaluation or local physical Mix87 testing claimed.

Fresh native Sheet read: Main/id0,1277 rows; Mix87 III C532 Supported, green
B532:C532 and strict dropdown confirmed. MAD68 V2 Dual remains low-quality/red.
All252 yellow models match the runtime catalog. No Sheet changes needed.
Private evidence: .local/release165-sheet-models.json and prior native readbacks.

EXE SHA256:e414302e83459608d86f296286aa5bee4b5b4e3c2369ac615b3771803fde884d.
Assets: HallJoy.exe, LICENSE, THIRD_PARTY_NOTICES.md, SHA256SUMS.txt.
Private corpus/captures/correspondence and three retired binaries excluded.
Hosted Windows remains workflow_dispatch-only; no dispatch will be made.

Publication result will be recorded after uploading and downloading assets.
