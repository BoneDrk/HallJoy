# Keyboards lost after Pause/Resume — shared routing re-claim bug (2026-09-30)

Tester report (MADLIONS MAD68 HE V2 Flagship): everything works perfectly,
including in game. After Pause and Resume, HallJoy shows the red "keyboard not
supported" banner.

## Cause (shared)

Every engine generation — startup and each Resume — runs
`EngineRuntimeEnumerateFresh`. It calls `NativeAnalogBackends_Reset`, which
clears all routing claims through `NativeAnalogRouting_Reset`, and then
`NativeAnalogBackends_PrepareRouting`. Three providers cached "already
prepared" and returned their earlier result without claiming again:

- `Hex80_PrepareProtocolRouting` (ATK Hex80, MAD68 HE V2 Flagship);
- `AddressedAnalog_PrepareProtocolRouting` (Addressed `09/94/02` family), which
  also kept a stale `g_claim`;
- AULA W669 `Prepare` (WIN 60/68 HE Standard, Redragon K673/K617 and others).

Their workers enumerate only routed interfaces, so after Resume they found
nothing. The keyboard stopped providing analog and the missing-source banner
appeared. AULA WIN60 HE already re-checked its claim and was not affected.

## Fix

- A latched prepare now reuses its result only while its claim is still
  present: `EnumerateCandidates(true)` / `Enumerate(true, …)` /
  `IsClaimedBy(retained path)`. Otherwise it clears the flag and proves and
  claims again.
- Addressed also drops its stale claim state before re-proving. The worker is
  stopped between generations.
- New `tests/native_resume_reclaim_static_audit.py` scans every backend for a
  latched prepare/routing function that returns early without re-checking its
  claim. It flags all three pre-fix functions and passes on the fix.
- Backup: `.local/backups/src-before-resume-routing-2026-09-30.tgz`.

## Validation

- Native checks (compiler): PASS, including the new audit
  (`latched_prepare_functions=4`).
- `build_release.ps1` EXIT=0; full-catalog exit 0.
- EXE SHA256 `5938dde6c7caa9fae1c4afd5c4f72c0b0899d767a9b4648aabed4c1299a528b0`.
- No hardware run. A physical Pause → Resume check on any affected keyboard is
  the confirmation.

## Follow-up: Hex80 re-proof failed after Pause (2026-09-30)

Owner report: the banner after Pause remained with the build above.

Cause: the fix made Hex80 prove the device again on Resume, but the proof itself
always failed. Pause sets the worker stop flag `g_stop`. The flag is cleared
only in `Hex80_Start`, which runs after routing is prepared. `Session::Request`
returned `false` whenever `g_stop` was set. So the Resume probe
(`ProbeCandidate`) sent its request and gave up at once:

- `hex80.probe` reported stage 3 (MAD68 V2 travel buffer) or stage 2 (Hex80
  travel info);
- no claim was made, `Hex80_Start` refused to start and the red banner stayed.

At startup the flag is still false, so only Resume was affected.

Fix:

- `Session` takes an optional cancel flag. The worker session passes
  `&g_stop`; the routing probe passes none. Worker cancellation is unchanged.
- The other re-proving backends were checked:
  - AULA W669, Addressed and MAD68 Pro R probes do not read their stop flag.
  - iRok NA87/ND75 claim from the worker after `Start` clears the flag.
- `native_resume_reclaim_static_audit.py` now also fails if the Hex80
  `Session` reads `g_stop` or the probe follows it. It fails on the previous
  code.
- `hex80_cooperative_shutdown_static_audit.py` checks that the worker session
  uses `&g_stop`.
- Backup: `.local/backups/src-before-hex80-probe-stop-2026-09-30.tgz`.

Validation:

- `research_reference_checks.py --record`: all original checks passed. It was
  needed because the 1.6.6 prep changed the notices json.
- `run_native_backend_checks.py --require-compiler`: PASS.
- `build_release.ps1` EXIT=0 (with the WinLibs PATH); full-catalog exit 0.
- FileVersion 1.6.6.0, SHA256
  `4c5887c5280f01120afe0f2442a0c650fb05c096b93273140a9e6e07c739e846`.
- No hardware run.

Hardware confirmation (owner, 2026-09-30): the MAD68 HE V2 Flagship tester
confirmed that Pause/Resume works with this build (no banner).
