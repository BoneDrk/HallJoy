# Firmware behavior review

Execution PASS means that observed behavior matches the test expectations. It
does not mean the protocol is suitable: a passing test may reproduce typing loss.
Use `tools/firmware_behavior.py` to validate a separate, evidence-backed review.

## Required review before proposing an integration

1. Pin exact firmware bytes, producer source and raw results. Record entry/stop
   boundaries, mocked peripherals and what the emulator does not model.
2. Inventory candidate paths, including rejected and unresolved paths. Explain
   their rejection and the limits of the search. Routing every opcode is not
   execution of every command payload, state or peripheral interaction.
3. Complete every behavioral finding: typing coexistence, simultaneous keys,
   range/resolution, shallow input, filtering, release, mapping, packet loss,
   persistent settings, normal exit, crash recovery and configurator contention.
   Each finding is established, restricted, unknown or justified not applicable.
   Assertions cite a pinned asset and a concrete locator. Unknown is not success.
4. Select the best reviewed usable path and explain why it is preferable to the
   alternatives. A limited path can remain useful with explicit red restrictions.
   Do not substitute calibration writes or binary events for live analog depth.
5. Present restrictions and unknowns before proposing a tester build. Do not
   describe a selected path as unrestricted based solely on execution PASS.

Static analysis and branch replay can establish scoped properties; a separate
physical tester for every model is not mandatory. Conversely, absence of a tester
does not erase unknowns. No report automatically promotes public support status.

## Commands and outputs

MAD68 example (output must be a new directory):

```powershell
python tools/mad68_v2_dual_firmware_audit.py .local/mad68-v2-review --report-dir .local/firmware-behavior/new-review
python tools/firmware_behavior.py .local/firmware-behavior/new-review/assessment.json
```

The bundle contains firmware inputs, producer source, raw results, static analysis,
`assessment.json` and a readable `REPORT.md`. Hash validation checks provenance,
not the truth of an assertion. `--require-ready` returns nonzero for limited,
incomplete and unselected paths; this is a suitability gate, not an execution test.
It must not discard useful limited candidates. The selected ID and restrictions
remain available in its JSON output.

Existing corpus execution receipts are not behavioral assessments. The corpus
report explicitly marks this distinction. Older suites still need an assessment
adapter; this implementation is not a universal MCU/peripheral emulator.

## MAD68 expanded regression, 2026-09-27

Three stock versions (1.06/1.07/1.09), 768 opcode routes, and 15,552 normalization
samples passed: all 144 indices, two settings, 18 depths including both sides of
thresholds. This exposed another limitation missed by the earlier sparse test:
indices 13,15,30,42,44,55,61 snap to 3250 above 3170/3070 depending on setting.
These are firmware indices, not a verified physical key mapping.

Command 36 remains the best reviewed limited analog source. Typing diversion,
shallow cutoff and delayed small changes remain restrictions. Factory binary
events, persistent calibration and saved-settings commands are not alternatives
of equal value. Complete recovery, real USB loss and concurrency remain unknown.
Local evidence: `.local/firmware-behavior/mad68-expanded-20260927/`.

No device commands, flashing, HallJoy compilation or support-status changes were
performed for this pipeline work. Owner retired the diagnostic build directory
for further compilation; historical trial paths/hashes are evidence, not current
build instructions. Future application builds use the ordinary build lifecycle.

Validation: `python -m unittest discover -s tools/tests -p "test_firmware*.py"`
passed80 tests, including7 new behavior tests. The final assessment was rendered
from the same verified raw replay (audit source unchanged) into
`.local/firmware-behavior/mad68-reviewed-20260927/`; pinned source/doc snapshots
reflect the final review adapter. `--require-ready` deliberately rejects its
LIMITED_ONLY result while preserving the selected command36 fallback.

## Shared replay and compact inspection — 2026-09-27

`tools/firmware_replay.py` provides the first shared strict Thumb execution core.
It maps the exact image read/execute and RAM read/write, with no automatic MMIO
mapping. Instruction/time limits, protected writes and execution outside image
bytes are failures. ReplayError contains fault address/access, PC, instruction
count and the last24 PCs. Hooks are removed even on faults so another reviewed
scenario can use the machine. RAM starts synthetic/zeroed: this is not proof of
real boot state, initialized memory, ADC, peripherals or hardware timing.

MAD68 now uses this core for dispatcher, commands, normalization and report branch
execution. Five isolated waveform scenarios preserve RAM between per-key passes:
slow press/release, shallow hold, threshold jitter, small changes/hold/release and
fast taps. They still use key index0 and modeled USB submission success. Full
keyboard lifecycle, simultaneous keys and fault-injected USB remain future work;
these scenarios do not establish those properties. Running the audit with Python
optimization is rejected because its existing assertions are required checks.

For agent research, start with the compact verified view:

```powershell
python tools/firmware_inspect.py .local/firmware-behavior/mad68-strict-replay-20260927
python tools/firmware_inspect.py .local/firmware-behavior/mad68-strict-replay-20260927 --pointer /0/stream_replay/32
```

The inspector validates evidence hashes first, groups identical sampled transfer
curves and references equal versions rather than repeating them. Sampled distinct
values are explicitly NOT sensor resolution. Raw JSON pointers retrieve exact
individual cases. Output over the character budget is refused with a narrowing
hint, never silently truncated. This adapter understands the MAD68 raw schema;
other suites need explicit adapters, not guessed field semantics.

Evolution policy: add reusable peripherals/scenarios when a real firmware needs
them; pin model assumptions and regression cases, and keep unknowns visible.
No universal-MCU claim or loss of evidence is acceptable for a smaller summary.

Validation completed:89 firmware-tool tests PASS, including9 strict replay and
inspection tests. Fresh MAD68 bundle `mad68-strict-replay-20260927` re-executed all
three images:15552 normalization samples plus2250 waveform passes. All15 scenario
runs end with an observed zero analog report; captured pairs match the reviewed
format. This establishes neither full HID restoration nor real USB reliability.
The new compact view is9925 characters versus971605 characters of raw JSON.
Default output fits its12000-character budget. Oversize refusal and exact pointer
retrieval were also checked. A requests dependency warning occurred in the global
Python environment; tests passed, but this is not a pinned-runtime certification.

Start subsequent investigations with the inspector, retrieve only the relevant
raw pointers, and extend the family adapter/core regression when an unsupported
behavior appears. Do not repeatedly paste entire raw sweeps into agent context.
Next priorities: modeled USB backpressure/failure, command-to-scan lifecycle,
ordinary HID recovery, simultaneous-key scenarios and explicit peripheral models.

## USB fault and mode lifecycle model — 2026-09-27

`firmware_usb_model.py` models only a reviewed submit function and RAM readiness
byte. It separately records attempted and accepted packets, injects nonzero return
values by call number, and changes readiness after an explicit number of firmware
reads (or never). It rejects unexpected endpoints/lengths. It is reusable with
explicit per-firmware addresses; no physical USB controller or timing is modeled.

`mad68_usb_lifecycle_audit.py` joins actual command36 enable/disable handlers with
the post-ADC processing branch in the same RAM instance. The bridge supplies the
mode byte as R0; the original scan caller is not executed. It then disables and
reenables the mode. Seven scenarios on each of three exact firmware images:
baseline, failure of each of the first four submissions, readiness after12 reads,
and permanently busy followed by modeled recovery. Output is a new hash-pinned
bundle with source/input snapshots, raw traces, summary and limitations. Source
changes during the run invalidate completion. An interrupted run has no manifest.

```powershell
python tools/mad68_usb_lifecycle_audit.py .local/mad68-v2-review .local/firmware-behavior/new-usb-review
python tools/firmware_inspect.py .local/firmware-behavior/mad68-usb-lifecycle-verified-20260927
python tools/firmware_inspect.py .local/firmware-behavior/mad68-usb-lifecycle-verified-20260927 --pointer /1/scenarios/6/passes/0/fault
```

Fresh21 scenarios reproduce the same findings on1.06/1.07/1.09:
- Permanent not-ready loops until the10000-instruction bound; restoring readiness
  permits continuation. This does not model interrupts that may restore readiness.
- Failure of either release fragment leaves only one accepted zero fragment;
  no retry of the missing fragment occurs in the next40 per-key passes. Other
  firmware retry machinery is not executed, so this is a scoped transport hazard,
  not a claim of a reproduced physical-device stuck key.
- Disable reaches ordinary processing entry; reenable sends an analog pair again.
  Ordinary keyboard HID delivery, crash/disconnect recovery and full scheduling
  remain unproven. Nonzero USB return injection is a hypothesis about controller
  failure, not an observed hardware event. No public support status changed.

Inspector verifies bundle assets and surfaces transport alerts automatically,
before compact traces. Existing behavior bundles remain supported. Tampering with
raw evidence is rejected.93 firmware-tool tests pass, including four new USB model
tests; the21 real-firmware scenarios have their own expectation checks. As before,
PASS reproduces these limitations; it does not approve a protocol as unrestricted.
