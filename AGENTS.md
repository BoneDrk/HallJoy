# HallJoy task completion rules

Read `docs/current/OWNER_CONTEXT.md`, then `docs/README.md` and relevant current
documents before work. Parent AGENTS.md instructions continue to apply.

## Mandatory support-status synchronization

Whenever support changes, tester feedback changes a support conclusion, a
restriction is removed, or a release declares keyboard support, follow
`docs/development/SUPPORT_STATUS_SYNC.md` in the same task.

Do not finish after editing only code or README. Reconcile the exact affected
models with docs/SUPPORTED_HARDWARE.md and the live Google Sheet, read back Sheet
values/validation/colors, and record the result in current documentation.
Before publishing a release, check every support change since the prior release.
The owner has authorized these corresponding Sheet status updates; do not ask
again. If Sheet access fails, explicitly retain and report a pending sync item.
No physical test per model is required solely to award Supported when the known
protocol and implemented compatibility are established. Do not invent testing.

## Repository organization

Keep versioned patch notes in `docs/releases/`, detailed compatibility in
`docs/SUPPORTED_HARDWARE.md`, commercial terms in `docs/legal/`, and contribution
guidance in `.github/CONTRIBUTING.md`. Read `docs/current/PROJECT_LAYOUT.md` before
adding root files. Keep correspondence marked local-only out of publication.

## Firmware analysis decisions

Before proposing a firmware protocol integration, follow
`docs/development/FIRMWARE_BEHAVIOR_REVIEW.md`. Separate execution PASS from
suitability; surface typing loss, state changes and unknowns. Compare alternatives
and retain the best reviewed limited analog path with explicit restrictions when
no unrestricted path is established. Do not infer exhaustive absence from bounded
emulation. Do not use the retired MAD68 diagnostic directory for new builds.

## Distribution provenance

Before publishing vendor firmware, captured configurator code or vendor artwork,
record the source-specific redistribution permission. A public download URL and
HallJoy's root license are not such permission. Keep unclear acquisitions local.
Consult `docs/legal/DISTRIBUTION_AUDIT_2026-09-27.md` for unresolved published
materials and runtime notices. Preserve original third-party attribution; never
claim exclusive ownership or royalty clearance merely from a scanner result.

## Publication after the 2026-09-27 history cleanup

Vendor firmware/configurator/art/manual/copied-code files with unestablished
redistribution rights remain local. Run tools/check_publication_inputs.py against
the publication Git index before every push; adding a source-specific permission
record requires review, not just a download URL or the root project license.
Old local mirrors/backups contain removed history. Base future publication on
the cleaned remote main; never force-push stale mirrors or reintroduce old tags.
Preserve existing release asset IDs and download counters; do not delete/reupload
historical EXEs for source-history or legal-document cleanup.
