# HallJoy 1.6.4 publication — 2026-09-26

Owner authorized this release. Public notes intentionally omit the private-use
K4 custom firmware work at the owner's request; its implementation stays included.
Title: v1.6.4: SteelSeries support and EPOMAKER layouts.

Changes since1.6.3: original full-size Apex Pro confirmed; original Apex Pro TKL
and full-size Gen3 experimental on firmware4.16.8; exact SparkLink discovery to
avoid Apex reset commands; UAP source-topology filtering; HE108 confirmed;
seven EPOMAKER layout source variants (six new visible choices); startup access
contention advisory and ordinary connection diagnostics. K4 r6 telemetry and
coalesced preview changes retained, backward-compatible with old firmware.

Fresh live Google Sheet read: Main/id0,1276 rows,647 models;252 experimental,
65 supported including custom-firmware and BR-tested qualifiers. All yellow rows
match runtime notices; strict dropdowns present. HE108 and original Apex Pro
green, TKL/Gen3 yellow, seven unresolved SteelSeries models gray. No Sheet writes
needed. Snapshot: .local/release164-sheet.json. Earlier task readbacks verified
base/effective colors and preserved surrounding formatting.

Version1.6.4.0 ordinary Release and six executable gates PASS. Current runtime
changes had full static checks, production-linked profiles/layouts,18 recovery
scenarios, startup health and actual worker-record regression PASS. Clean source mirror full static audits PASS
(.local/release164-clean-static.log). Windows hosted CI remains opt-in;
no workflow_dispatch is used. No forced diagnostic logging.

EXE SHA256:5948c5286b684437c247aae0cb6db483c75aa0c65df4497f7272ea729b36bbd5.
Assets: HallJoy.exe, LICENSE, THIRD_PARTY_NOTICES.md, SHA256SUMS.txt.
Source publication excludes local data/build outputs, credentials and private
Pwnage correspondence. Prior mirror files backed up in
.local/backups/release164-mirror; candidate publisher preserves replaced EXEs.

Published stable/latest at2026-09-26T19:07:04Z:
https://github.com/PashOK7/HallJoy/releases/tag/v1.6.4
Source/tag target:3af768e949f6aef4fd8ddad58694fe3368f74da3.
Downloaded public EXE matches local SHA256 and size10,001,408 bytes.
Public release body verified: no K4 mention. All four assets uploaded.
GitHub run36264899801 Windows job SKIPPED; portable job still running at
verification, independent of successful local validation.
