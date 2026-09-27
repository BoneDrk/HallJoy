# Shared diagnostics overhaul, 2026-09-27

Owner request: standardize diagnostics for all protocols after log36's false-zero
summary. No keyboard support status change or GitHub publication.

## Implemented

- One compiled manifest sets BOTH lifecycle and telemetry capacity. Removed the
  separate lifecycle32 cap as well as the previous telemetry16 cap.
- Production shared collector visits the complete catalog, tracks expected/visited/
  unavailable counts and preserves missing data as unavailable. Diagnostic mode
  includes absent providers and lifecycle state/generation/errors; UI mode retains
  its compact active list and does not acquire lifecycle locks.
- Schema2 snapshots include sequence-linked begin/end, every provider row,
  coverage, and source counts from the same collection. UI cached observations
  are labelled explicitly. Background prehistory also keeps the full snapshot.
- Strongly typed optional event details distinguish data, Win32 and protocol
  errors. Migrated all production three-argument calls. Corrected the Shark
  self-test that previously expected a repeated board ID and usage as errors.
- Added a read-only analyzer that rejects missing/duplicate rows and inconsistent
  summary data; legacy logs cannot establish complete catalog coverage.
- Ordinary Release requires the executable diagnostics contract gate before any
  running application is touched. Full BUILD runner also contains these tests.
- Mix87 stale hardware-test-pending runtime wording removed following the owner's
  tester confirmation. No protocol behavior/calibration/USB transactions changed.

## Evidence and limitations

Native collector regression, actual Windows writer in ordinary and input-path
variants, Mix87 and MAD68 fake-HID sessions, analyzer fixtures and targeted static
checks PASS. Ordinary Release six executable gates and embedded legal/ABI1 byte
checks PASS; verified EXE installed in build/bin/Release/x64/HallJoy.exe.
Full static runner PASS. Installed SHA256 89e201d9eeb1fb4ea3d59e27fe9dd559c3f9fabd300fad539ebb879e114caf2e.

Private evidence: .local/research/mix87-20260927/diagnostics-contract-build-final.log,
diagnostics-contract-static-final.log. Earlier build log retained: first candidate
was correctly blocked by Shark's obsolete metadata-as-error expectation; installed
EXE was untouched until the corrected candidate passed.

Backups: .local/backups/diagnostics-contract-20260927-182755.zip and
diagnostic-event-types-20260927-183451.zip.

No claim of atomic device snapshots, uniform counter scope, or hardware testing
on every supported keyboard. Firmware-specific failure reasons still depend on
backend instrumentation; unknown remains unknown. No new forced logging for
limited/unstable warnings, no per-key values, no additional USB polling, no changes
to gamepad processing. Bounded history can truncate; the analyzer detects missing
snapshot structure rather than silently interpreting missing devices.

Contract: ../development/SUPPORT_DIAGNOSTICS_CONTRACT.md.
