# Dependency provenance and ABI0 retirement — 2026-09-27

Local work; no GitHub push, tag update, release upload or asset deletion in this task.
The published historical cleanup is documented separately in
[HISTORY_CLEANUP_2026-09-27.md](HISTORY_CLEANUP_2026-09-27.md).

## Production dependency changes

- HallJoy embeds ABI1 only. The plugin builder now builds/packages ABI1 only and
  does not stage legacy `.lib`/`.a` inputs. Wooting support remains enabled in ABI1;
  this does not retire UAP keyboards or the Keychron UAP firmware path.
- Legacy `wooting_analog_common.lib`, `wooting_analog_common.a` and the unused
  `WootingAnalogSDK091/lib/wooting_analog_sdk.dll.lib` remain locally for rollback.
  They are ignored and denied by the next-publication input policy. Their removal
  from the current public tree is NOT yet published. Old upstream ABI0 recipes
  remain as source/history, but the production builder does not use them.
- New ABI1 DLL: SHA256
  `0ed0f53d0935b7c94c2e796d88c48ed1abc81484075fc05c0f5b5e5b748532cf`.
  Its actual linker map contains exactly seven Soup objects: AnalogueKeyboard,
  DigitalKeyboard, Process, Thread, alloc, base, hwHid. No legacy Rust common or
  TinyPngOut was found in this link map. The retained map and DLL are hash-bound
  by `tools/verify_uap_link_closure.py`; changed membership fails the build.
- Full plugin builds retain map/record alongside output and copy them to
  `build/runtime/` with ABI1. Incremental Release checks that exact record before
  compilation. Embedded-resource verification also compares the actual ABI1 bytes
  in the finished EXE. Plugin ZIP includes the MIT license and third-party notices.

## ViGEmClient

- Rebuilt from upstream commit `b66d02d57e32cc8595369c53418b843e958649b4`, MIT.
  All four existing public headers match that revision after newline normalization
  and, for Client.h, the CP1252/UTF8 encoding conversion.
- Release_LIB x64, MSVC v143 / 14.44.35207. Installed library SHA256
  `2d99c44ed8f615eb7a481f8c13781b25d883f07c006ceecb9c694369c969364e`.
  All 30 exported public `vigem_*` library symbols match the previous library.
- `tools/dependency-lock.json`, the component LICENSE and PROVENANCE.json record
  source, toolchain, hashes and API inventory. `tools/build_vigem_client.ps1`
  acquires the immutable source revision, rejects a dirty cache and produces a
  candidate without silently replacing the locked library.
- Fresh cache clone/build using that helper passed. Its hash differs because the
  build/source paths affect debug metadata; bit-identical reproduction across
  directories is not claimed. The checked-in candidate's actual hash is locked.

## Wooting SDK header

- `include/wooting-analog-sdk.h` matches the upstream v0.9.1 generated header
  exactly after CRLF/LF normalization, commit
  `be67cbf479eb1e10e2859e71dbdcc12fff7ba266`.
- Component LICENSE contains MPL-2.0; PROVENANCE.json pins the header hash and
  upstream path. Covered source stays supplied with HallJoy. This evidence is for
  the header, not the unused old import library or Rust archives.

## Validation and delivery

- Fresh ABI1 source build passed without copying the legacy common archives.
- Four closure-verifier tests passed, including modified DLL/map rejection.
- Four publication-policy tests and dependency-lock static audit passed.
- Full static audit sequence passed across the recorded initial/resumed runs.
  Updated the stale support-log audit to check the shared policy (including no
  automatic logging for communication warning/limited firmware). Registered the
  existing Windows-only MAD68 fake-HID session test in the ordinary runner;
  compiled and executed it successfully. No runtime code was changed for these
  two test-maintenance corrections.
- Final incremental build repeated all six gates after adding the ViGEmClient
  size/hash preflight. The existing allowlisted LNK4099 warning means matching
  ViGEm debug symbols are unavailable; runtime linkage completed successfully.
- Physical private ABI check on the new DLL passed: one device, 134 samples,
  dual-view equivalence, negotiated capacity and unload lifecycle.
- Ordinary Release, six existing candidate gates, exact embedded license/notice/
  ABI1 verification and ZIP name/hash round-trip passed.
- Installed ordinary EXE: `build/bin/Release/x64/HallJoy.exe`, SHA256
  `e839d000ea71d1ca02718eb6cf4bff06e76385659bc4ad28a289c55489c4ece1`.
- No visual evaluation or per-model hardware retesting is claimed. Support
  statuses did not change, so no Sheet/README support promotion was performed.
- General project-layout check reports the pre-existing local `.analysis/`
  backup directory; this is not a dependency build failure. It remains private
  and excluded by publication policy; no backup was deleted to make a check pass.

Evidence is local under `.local/license-cleanup-20260927/`: abi1-only-build.log,
provenance-release-build.log/.exit, incremental-provenance-check.log,
abi1-package-final, dependency-static-checks-final.log and
 dependency-static-checks-resumed-final.log. The earlier package/check logs are
historical checkpoints, not the final installed EXE.
Backups are under `.local/backups/`: abi0-retirement-1790512047428465900.zip,
vigem-provenance-1790512425038480300.zip and runtime-provenance-1790512809075171500.zip.

## Scope and remaining work

The exact new build has stronger provenance. This is not a retroactive proof of
old shipped stripped DLLs, ownership of third-party code, or universal legal
clearance. Earlier release EXEs/download counts were not touched. Any focused
publication must omit the three retired binary paths, retain covered sources and
licenses, pass public-source checks and avoid publishing unrelated local features.
Do not force-push the old mirrors after the historical cleanup.
