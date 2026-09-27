# Research corpus publication cleanup — 2026-09-27

The owner authorized history/tag cleanup while preserving existing GitHub
releases, their uploaded assets and download counts. This is a conservative
redistribution policy, not a finding that every excluded file infringes rights.

## Scope and verification

- 553 acquired firmware/configurator/artwork/manual/copied-code paths removed
  from all 139 historical commit trees. All other historical file objects and
  modes verified identical, with all 15 version tags and main retained.
- A full original Git mirror and a separately hash-verified private corpus ZIP
  are preserved locally. Research remains available to the maintainer.
- No release or uploaded asset is deleted or reuploaded. Asset IDs, names,
  digests, sizes and counters are recorded before and checked after publication.
  Counters may grow while the operation runs, but must not decrease.
- Commit hashes and auto-generated Source code archives change. Existing
  uploaded EXEs are unchanged. Local mirrors/forks may retain earlier objects;
  rewriting refs does not erase third-party copies or guarantee cache removal.

## Source-evidence checks without republishing vendor software

`python tools/research_reference_checks.py --require-private` runs the full
source-backed checks. `--record` requires all original checks (including layout
unit tests and all 16 brand generators) to pass before producing the public
reference manifest. Read dependencies are traced; absent private data never
counts as a fresh source replay. When private inputs are unavailable, public
checks verify unchanged tool/input/output hashes and explicitly report that
private source replay was not run. Changed or missing public inputs fail.

Portable C++ tests and architecture checks remain separate. The public tree
passes its static runner without the private corpus. Historical tags retain
their original tools; old source-evidence audits can require private captures.
The deleted research files are not linked application source or runtime inputs.

## Licensing changes and limits

Future builds embed LICENSE and third-party notices and expose a Licenses viewer.
ZIP packaging verifies exact embedded notice bytes and includes separate copies.
Eight shared firmware headers have the owner-approved GPL alternative; the
Windows application's license has not changed. Keychron attribution is retained.

This cleanup does not certify complete legal clearance. Remaining audit items
include exact legacy ABI0 binary/source provenance and complete dependency
closure for old shipped binaries. See DISTRIBUTION_AUDIT_2026-09-27.md.

## Published result

Atomic leased update of main and 11 affected tags succeeded on 2026-09-27.
Post-push readback verified all 15 release IDs and all 48 asset IDs, including
15 EXEs. Names, sizes, available digests, download URLs, creation/update times
of assets and release descriptions remained unchanged. Download totals were
520 before and 520 after; every per-asset counter was checked for non-decrease.
Four earlier tags were unchanged. No release-management write API was used.

Validation:139 historical trees compared file-by-file; complete private source
checks and layout tests; public static runner; clean publication Release build,
six linked-image gates and embedded-license byte verification. Hosted Windows
CI was not used. Local evidence is in .local/license-cleanup-20260927/.
