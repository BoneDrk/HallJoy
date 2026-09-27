# Support diagnostics contract (schema 2)

## Source of truth

The compiled `native_analog_backends.def` manifest determines both lifecycle and
telemetry capacity via `native_analog_catalog_size.h`. There is no independent
16/32-entry limit. `CollectNativeAnalogTelemetry` is shared production code with
a direct executable regression test. Ordinary UI collection retains only active
entries; background diagnostic collection includes EVERY entry, even absent or
unavailable ones, and reads registry lifecycle state/generation/error/operation.
No collector issues USB transactions. Lifecycle reads happen only in background
diagnostics, not on the realtime or UI path.

`Backend_GetAnalogDiagnosticTelemetry` captures one bounded observation sweep.
Providers are read separately, not under a global lock: this is NOT an atomic
cross-device snapshot. Summary counts derive from that same sweep. UI search and
connection observations have `_cached` suffixes and never override fresh evidence.
`none_reported` means no provider reported connection; it is not proof that a
physical keyboard or a usable firmware protocol does not exist.

## Complete reports and unknowns

Every periodic snapshot (30 seconds, including bounded in-memory history when
logging is off) includes begin/end markers with a matching sequence, coverage
counts and one row per compiled backend. Triggered snapshots use the same format.
Inventory is Windows metadata only. Each row carries stable protocol ID, catalog
index, USB/report metadata, provider availability, observed connection, counters,
and separately available lifecycle evidence. No key values/text, serial numbers,
user device names, paths, or raw packets are serialized.

`unavailable`, `not_present`, `present_not_connected`, and `connected` are distinct.
Lifecycle `running` means worker startup succeeded, NOT analog reception.
`lifecycle=unavailable` remains explicit even if ordinary telemetry is available.
`complete` describes coverage/availability of telemetry; lifecycle availability
is a separate per-row field. Counters retain each provider's existing scope; do
not subtract across reconnects or assume every provider counts complete keyboard
frames. A zero counter/age must never alone establish failure. Long idle periods
are valid for change-driven streams; there is no generic silence timeout.

Queue/history/file bounds remain 512/512/4 MiB. Reports may be truncated by those
bounds or concurrent event bursts. The analyzer verifies begin/end, expected rows
and unique catalog indices rather than declaring missing rows absent. Queue loss
is explicit. Raw logs remain available even when automated interpretation fails.

## Typed event details

`SupportLog_Event(category, value)` is a structural event.
The optional third argument MUST be `SupportLog_Data(...)`, `SupportLog_Win32(...)`
or `SupportLog_Protocol(...)`. An untyped third integer is a compile error.
`detail_kind` identifies interpretation. Data details always emit `error=0`;
nonzero board/profile/count values are not errors. Win32/protocol codes are separate
domains. A backend may have a protocol-specific payload meaning documented locally;
never infer that a nonzero event value itself means failure. Existing multi-value
call sites were explicitly classified, not automatically treated as Win32 errors.

## Analysis and regression gates

Run `python tools/analyze_support_log.py <log>` for JSON evidence: latest snapshot,
structural/telemetry completeness, source-summary consistency, and queue loss.
Legacy schema1 logs are explicitly unverifiable for catalog coverage. The tool
never infers a faulty keyboard, closed configurator, or failed analog from silence.
Tester statements are separate evidence; preserve them even if telemetry differs.

`tools/build_release.ps1` requires `tools/check_support_diagnostics.py` before
compilation/publication. It compiles the production collector, real Win32 writer
(normal and diagnostic variants), and affected fake-HID sessions. C++20 g++/clang++
(or CXX override) is required locally; no GitHub Windows workflow is involved.
The normal native-backend runner includes these tests and analyzer fixtures too.

Regression cases: connected provider beyond index16, missing telemetry, overflow
mismatch, absent providers, unavailable lifecycle, idle event stream, duplicate
or truncated log rows, contradictory summary, and legacy zeros. Writer coverage
includes no forced file for limited/unstable warnings, mirror behavior, privacy,
write failures/recovery, bounded overflow and portable single destination.

## Adding a protocol

Add the descriptor to the manifest. Implement meaningful generic telemetry with
read-only atomics/snapshots, and never use UI refresh as a diagnostic data source.
Keep `present` separate from confirmed live analog; document what update counters
and age represent. Log protocol-specific admission failures using typed details.
Unknown cause must remain unknown, not an invented driver-conflict diagnosis.
Tests must cover admission, normal samples, malformed replies and disconnects.
Passing host tests does not prove hardware behavior or guarantee all future bugs
are impossible. New firmware-specific gaps require extending the backend evidence.
