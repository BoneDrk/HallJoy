# Distribution and ownership audit — 2026-09-27

**Result: not cleared for an unrestricted "everything is ours" claim.**
No mandatory per-copy royalty was identified in the reviewed MIT, BSD-3-Clause
and MPL-2.0 licenses of the principal runtime components. That is not a patent
clearance, a guarantee against claims, or verification of every transitive binary
component. The public research collection has unresolved redistribution rights.

## Scope and evidence

- Complete GitHub `main` tree: `a3c2a75b790c9ae906fbdb0978b8f65b2052a84f`,
  2,645 file entries; recursive API response was not truncated.
- All entries inventoried and local bytes hashed. At initial capture, 2,604
  matched published bytes exactly or after CRLF normalization. The other 41 were
  project-material paths, not the vendor/runtime evidence reviewed below.
- Current local build definitions, resources, third-party source and binaries,
  packaging scripts, firmware patch, commercial terms and contribution terms.
- Release list inspected; latest release v1.6.4 supplies EXE, LICENSE, notices and
  checksum assets. Its source-tree notice still linked v1.5.1; corrected locally.
- No exhaustive all-history/all-tag binary audit, full clone-lineage analysis,
  patent/trademark search or legal opinion. A category in the inventory is a
  review queue, not a legal conclusion about all files in it.
- Evidence: `.local/license-audit-20260927/` (GitHub tree, release metadata,
  upstream license responses, initial per-file `inventory.json`). Reproduce with
  `tools/audit_distribution_inventory.py`; it never labels its output legal clearance.

Initial inventory categories:

| Category | Files | Meaning |
| --- | ---: | --- |
| Project license declared | 1,673 | AGPL declaration; not independent proof of authorship |
| Vendor redistribution not established | 492 | Requires source-specific permission or publication exclusion |
| Research provenance review | 415 | Mix of upstream code/data and generated research; not all proprietary |
| Third-party runtime conditions | 53 | Component licenses apply independently |
| Separate firmware GPL conditions | 5 | QMK/Keychron-derived firmware work |
| Project artwork provenance | 5 | Subsequently owner-confirmed Nano Banana generation |
| Archives requiring member inspection | 2 | Both member lists inspected; one contains a vendor firmware image |

## Application components

| Component | Evidence and rights | Outstanding action |
| --- | --- | --- |
| HallJoy application | Root AGPL-3.0; matching tagged source distributed through GitHub. Contributor API lists one account, PashOK7; this does not prove every line's exclusive ownership. | Keep exact release source and build inputs available. Do not apply HallJoy ownership to external code. |
| Universal Analog Plugin | Local `LICENCE`: MIT, Calamity Inc. Upstream now at AnalogSense/universal-analog-plugin, also MIT. | Preserve copyright/permission notice for local modifications and redistributions. MIT is permission, not transfer of ownership. |
| Soup | MIT at pinned commit b02796b0b20276277c8a4b4d3759643eeab43ff7; seven local overlay files locked. | Complete transitive notice closure. Source tree includes third-party notices for, e.g., Nayuki QR, compression and crypto implementations. Final linked object set was not established; do not assume its top-level MIT text replaces these notices. |
| Sun | MIT build tool pinned at 83c195bd61314bdbfdccc161653dbb652e3b6678. | Build-time tool, not the application payload; preserve notices if packaging the tool. |
| Wooting common/SDK | MPL-2.0 upstream; linked common archive and SDK declarations. MPL permits distribution with its conditions, including source availability for covered files. | Establish exact corresponding source revision and transitive Rust notice set, not only a moving upstream URL. No closed-source waiver for MPL-covered modifications. |
| ViGEmClient | MIT headers; locked library SHA matches local bytes. | Keep full MIT notice; improve exact binary-to-source build provenance. |
| ViGEmBus 1.22.0 | BSD-3-Clause license preserved; installer embedded in HallJoy.rc and pinned by hash/size. | Preserve BSD notice and no-endorsement condition; installer payload/toolchain notices remain part of exact-binary review. |
| MSVC/Windows runtime | Application uses v143 and static release CRT (`MultiThreaded`), not Debug CRT. Windows SDK system libraries are linked. | Microsoft distributable-code terms apply; they do not become HallJoy-owned. Build-tools/IDE licensing is separate from royalties. No installed-license entitlement adjudication performed. |
| Application artwork | Owner states all HallJoy logos/icons were generated in Google Nano Banana. Five files: two ICO and three HallJoy-Discord PNG variants. | Record owner provenance. Google terms say it does not claim ownership of generated content; this does not guarantee uniqueness, copyright protection or trademark clearance. Exact generating service/date/prompts not recorded. |

Wooting common binaries match upstream UAP files byte-for-byte (downloaded for
comparison, not executed):

- `wooting_analog_common.lib`: 11,942,102 bytes,
  SHA256 `5a7c996e8dd25669477a2e05b603632c48f7a3f48a2eee3ed676aa17b92ba30b`.
- `wooting_analog_common.a`: 22,790,496 bytes,
  SHA256 `d7605b59e85bc0e14d7d3221978c554bc57941131256905c94761e9fca9f05c5`.

This proves byte provenance, not complete MPL compliance. Embedded build-path
strings identify candidates including ffi-support 0.4.4, serde 1.0.136, log 0.4.17,
lazy_static 1.4.0, num-traits 0.2.15, libc 0.2.147, hashbrown 0.14.0,
miniz_oxide 0.7.1 and Rust runtime components. These are evidence leads, not a
complete or linker-verified SBOM. Rebuild from pinned source/dependencies or obtain
the exact source and notices before marking this gap closed.

## Public research material: priority publication review

The 492 higher-priority entries comprise 419 JS, 33 BIN, 13 PDF, 7 JPG, 6 CSS,
4 HTML, 3 SVG, 2 HEX, 2 PNG, one GZ, one ASM and one ELF. These are not 492 proven
violations: permissions have not been established for whole captured files.
An embedded MIT library inside a vendor JS bundle does not license the entire
configurator. Download access and a source URL do not establish redistribution.

Examples already public:

- `docs/firmware/io-type84-magnetic/*.hex`, MCHOSE ACE68 Pro image;
- `docs/research/ipi-firmware-20260914/*.bin` and Keychron firmware images;
- Sayo original/decrypted image, ELF and full disassembly;
- captured configurator JS/CSS/HTML, Razer manuals/SVG/photos and Redragon photos;
- `MAD68_Pro_R_Firmware_Knowledge_Base_2026-07-29.zip`: 81 archive entries,
  including a vendor firmware image and research/capture material.

`docs/archive/legacy-addressed-analog-v5.zip` contains four HallJoy build/readme/
validation files, not a vendor firmware image. It should not be confused with the
MAD68 archive solely because both are ZIP files.

The additional 415 research entries need provenance review too: copied upstream
C/Rust/C# code, catalog JSON, mappings, INI data, analyses and captures differ in
rights. Keychron source headers identify GPL-2.0-or-later; GPL permission does not
license unrelated vendor binaries. For unclear material, preserve private research
copies and publish project-written protocol facts, acquisition URLs/hashes and
scripts where permitted. Do not erase evidence as part of cleanup.

Removing files from current main does not remove old tags, commits, archives,
forks or releases. No history rewrite or remote deletion was performed in this
audit. Any historical cleanup needs a deliberate plan, not a claimed guarantee.

## Separate Keychron firmware

`firmware/keychron_k4_he` targets Keychron/qmk_firmware commit
ee7390c3bbdc1f71a1cc8d54323f3f1d97868593. Its C files declare GPL-2.0-or-later.
It is not part of the Windows EXE and is not exclusively HallJoy-owned software.
Before distributing a firmware binary, preserve corresponding source, changes,
build material and applicable QMK/submodule/library licenses.

Specific remaining issue: `base.patch` replaces the original Keychron copyright
header in `usb_descriptor_override.c` with only an SPDX line. Preserve the original
notice for the derivative file. Shared HallJoy headers compiled into firmware
also need an explicit, compatible licensing grant from their actual rightsholder;
do not assume a commercial HallJoy agreement waives QMK/Keychron obligations.

## Distribution workflow and ownership wording

- Root notices referenced v1.5.1 even in v1.6.4: corrected locally to require the
  matching release tag. Also corrected five Soup overlay files to seven.
- Both commercial-license documents now explicitly limit the grant to rights the
  owner can license and retain all third-party obligations. Existing contribution
  terms are not evidence of assent from an earlier or external author.
- `tools/package_release.ps1` still reads obsolete `build/release` and omits
  LICENSE from its package file list. `tools/build.ps1` copies dependency-lock and
  notices, but not LICENSE in its final copy section. Current GitHub release assets
  do include LICENSE: distinguish actual release from an unsafe future script path.
  Packaging remediation is still required; no build/publish was run here.
- The EXE does not embed the full license documents in HallJoy.rc. They accompany
  releases separately. Forwarding EXE alone is not a reliable way to preserve all
  notices. Embedded/readable notices can preserve single-EXE convenience without
  adding runtime writes beside the EXE; that implementation is still pending.
- Internal research dependencies (including Unicorn) and standalone diagnostic
  runtimes need their own license inventory if distributed as tools. They are not
  automatically Windows application dependencies. No corpus publication authorized
  by this audit's existence.

## Required closure before a clean distribution claim

1. Remove unclear vendor payloads from future publication inputs after preserving
   local evidence; verify per-source permission for anything retained publicly.
2. Resolve Wooting exact-source/transitive notices and Soup linked-code notices.
3. Repair license packaging and preserve legal documents for single-EXE delivery.
4. Preserve Keychron attribution and define firmware/shared-header license scope.
5. Review previously published versions separately; retain an explicit unresolved
   list until evidence exists. Do not mark unknown rights as either forbidden or cleared.

## Primary references

- [MPL-2.0 text](https://www.mozilla.org/en-US/MPL/2.0/) and
  [Mozilla FAQ](https://www.mozilla.org/en-US/MPL/2.0/FAQ/): source/notice duties.
- [ViGEmBus BSD license](https://github.com/nefarius/ViGEmBus/blob/master/LICENSE),
  [ViGEmClient MIT license](https://github.com/nefarius/ViGEmClient/blob/master/LICENSE).
- [Universal Analog Plugin](https://github.com/AnalogSense/universal-analog-plugin),
  [Wooting source/license](https://github.com/WootingKb/wooting-analog-sdk).
- [GNU license FAQ](https://www.gnu.org/licenses/gpl-faq.en.html): distribution,
  commercial use and copyright-owner licensing; no transfer of third-party ownership.
- [Microsoft redistribution rules](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170).
- [Google terms](https://policies.google.com/terms),
  [Gemini API terms](https://ai.google.dev/gemini-api/terms): generated content.
- [GitHub DMCA policy](https://docs.github.com/en/site-policy/content-removal-policies/dmca-takedown-policy).

Local validation: dependency-lock static audit PASS after notice corrections.
Repository classification is reproducible, but legal conclusions are not generated
by a filename scanner. No runtime behavior, public support status, firmware,
GitHub release or published history was changed.

## Remediation checkpoint: shared firmware headers (2026-09-27)

Owner approved the firmware compatibility grant after a plain-language
explanation. Eight headers in the firmware include closure now carry
AGPL-3.0-only OR GPL-2.0-or-later. The Windows application is not relicensed.
Exact scope: firmware/keychron_k4_he/LICENSING.md. GPL text is included as
COPYING. Original Keychron attribution restored in descriptor source/patch.
Other required closure items remain open. No flashing or public changes.

## Remediation checkpoint: embedded notices and publication review

- Local ordinary EXE now embeds exact LICENSE and THIRD_PARTY_NOTICES.md bytes.
  Global settings > Licenses opens an in-memory read-only viewer; no documents
  are written beside EXE or to AppData. Packaging still supplies separate notices.
- Canonical build/package path corrected; LICENSE included; checksum generated
  from the actual candidate. Both build wrappers verify embedded legal resources
  before replacement. Windows CI artifact list includes LICENSE.
- HIDAPI BSD alternative and complete MPL text added to notices. The notice
  inventory is not yet a claim that all provenance/source obligations are closed.
- Eight shared firmware headers grant and complete base.patch check PASS.
- Ordinary candidate passed six existing checks and embedded-resource comparison.
  Elevated standard publisher succeeded and installed/restored the application.
  SHA256 B87FF03B11DA76381C86CB370E5B9B424C89F8CAAF5DA0CCDD8A54A576879E42.
  Package round-trip names/hashes PASS, including LICENSE. Visual review not run.
- ABI1 audit relink map contains seven Soup objects: AnalogueKeyboard,
  DigitalKeyboard, Process, Thread, alloc, base, hwHid. No TinyPngOut, Wooting
  common or Rust symbols. This is an audit relink, NOT an exact-byte proof for
  previously shipped stripped DLLs: its text/rdata differ from installed runtime.
  Evidence build/obj/UAP/native/license-audit.map. ABI0 legacy binaries remain
  separately unresolved and must not be treated as cleared by this result.
- Full Git mirror preserved at .local/license-cleanup-20260927/remote-backup.git.
  Verified private archive contains 553 selected firmware/web/art/manual/copied
  code paths; hashes and historical per-ref exposure retained beside it.
- Separate publication-review checkout excludes those 553 paths. Publication
  input checker and three negative/positive tests PASS. Ignore rules and CI gate
  prevent accidental reintroduction of known paths and common payload formats.
  This is a conservative publication policy, not a finding of infringement.
- IMPORTANT: cleanup candidate is NOT publish-ready. Its portable runner still
  requires private vendor JS (first failure: check_rongyuan_stream_profiles.py).
  Refactor source-evidence audits separately from public generated-catalog checks
  without silently weakening the full private audit. Do not publish this candidate
  or claim clean-source CI passes before this is resolved.
- No public changes. Historical rewrite permission requested separately because
  old commit/tag hashes and generated source archives would change.

Remaining: public/private audit separation; all copied-code/catalog provenance;
ABI0 exact source/transitive review or removal from publication/build outputs;
exact shipped Soup/ViGEm closure evidence; historical cleanup and release assets.

## Published historical cleanup result

2026-09-27: owner authorized cleanup with download preservation. Published main 4af3fcf4f3f49752500926cf670ea46a012102f5. All15 releases,48 asset IDs and15 EXEs remain unchanged;520 downloads before/after and no per-asset decrease. Main and11 affected tags verified remotely. Full private source/layout audits and public hash-reference/static checks plus clean publication Release gates PASS. Source-evidence separation resolves the earlier public-runner blocker. See HISTORY_CLEANUP_2026-09-27.md and local final-verification.json. Unrelated local runtime features were not published. Remaining binary/source provenance items are not declared closed.

## Local dependency provenance follow-up

See [DEPENDENCY_PROVENANCE_2026-09-27.md](DEPENDENCY_PROVENANCE_2026-09-27.md) for the newly built exact ABI1 closure, pinned ViGEmClient source build, verified Wooting v0.9.1 header, retired binary publication policy and passing Release/package checks. Local only; old shipped binary provenance is not retroactively established.
