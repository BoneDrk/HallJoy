"""Exact MAD68 suite adapter. Output is a self-contained immutable review bundle."""
import hashlib
import json
import shutil
from pathlib import Path
import platform
import unicorn
from firmware_behavior import CHECKS, validate, render

def write_report(directory, results, inputs):
    directory=Path(directory)
    directory.mkdir(parents=True, exist_ok=False)
    assets={}
    def add(name, source, role):
        data=Path(source).read_bytes();target=directory/Path(source).name
        with target.open("xb") as f:f.write(data)
        assets[name]=dict(path=target.name,sha256=hashlib.sha256(data).hexdigest(),role=role)
    for name in ("firmware_behavior.py","mad68_firmware_behavior.py","mad68_v2_dual_firmware_audit.py","firmware_replay.py") :
        add(name,Path(__file__).parent/name,"producer-code")
    from mad68_v2_dual_firmware_audit import VERSIONS
    for version,cfg in VERSIONS.items(): add(version,Path(inputs)/cfg["file"],"firmware-input")
    data=json.dumps(results,indent=2).encode()
    with (directory/"raw-result.json").open("xb") as f:f.write(data)
    assets["raw"]=dict(path="raw-result.json",sha256=hashlib.sha256(data).hexdigest(),role="raw-result")
    def finding(state,detail,locator=None):
        return dict(state=state,detail=detail,evidence=[dict(asset="raw",level="branch_replay",locator=locator)] if locator else [])
    findings={k:finding("unknown","Not exercised by this selected-branch suite.") for k in CHECKS}
    findings.update(
        normal_typing=finding("restricted","Simulation diverts the per-key path away from ordinary processing; this is not a parallel read-only stream.","normal_branch_reached / simulation_samples; exact mode branch addresses in producer"),
        depth_scale=finding("restricted","Values clamp at3250. Indices13,15,30,42,44,55,61 snap to3250 above3170 (setting0) or3070 (setting1). Physical key mapping not established by these indices.","simulation_samples: all144 indices, both settings, top boundaries"),
        shallow_input=finding("restricted","Values below200/400 are discarded; special indices use260/460. These are raw units, not measured millimetres.","simulation_samples: below/at each threshold"),
        temporal_filtering=finding("restricted","1500->1510 withheld until pass32;31 additional per-key passes. No milliseconds inferred.","stream_replay[1:33]"),
        release=finding("established","Tested large transition tozero emits zero immediately in this branch. Other release waveforms and physical timing remain untested.","stream_replay[-1]"),
        persistent_settings=finding("unknown","Command36 replay writes only mapped RAM, but does not execute the complete scheduler/USB lifecycle; persistence outside the stopped branch remains unproven."),
        normal_exit=finding("unknown","Mode bit clears in command36 branch; restoration of all ordinary HID behavior is not emulated end-to-end."),
        crash_recovery=finding("unknown","No autonomous timeout or full MCU reset/recovery simulation established."),
        packet_loss=finding("unknown","Two same-key six-bit fragments have no observed sequence marker. Loss/reordering has host tests but is not injected into MCU USB transport here."))
    report=dict(schema=1,model="MADLIONS MAD 68 V2 Dual",scope="28E9:3265; stock1.06/1.07/1.09; synthetic inputs; not full-device emulation",
        execution=dict(outcome="pass",entry_boundary="dispatcher / normalized per-key input (ADC conversion bypassed)",
                       stop_boundary="handler boundary / per-key return; no complete boot or scan loop",
                       mocked=["RAM seed","USB submit returning success","input depths"],
                       not_modeled=["ADC","real USB timing/backpressure","interrupt scheduling","flash persistence","crash recovery"],
                       python=platform.python_version(),unicorn=unicorn.__version__),assets=assets,findings=findings)
    report["candidates"]=[dict(id=id,disposition=state,reason=reason,
        evidence=[dict(asset="raw",locator=locator)]) for id,state,reason,locator in [
        ("36-simulation","limited","Only reviewed path with actual per-key depth and volatile enable/disable. Typing loss and value filtering accepted only as explicit red limitations.","command_replay,simulation_samples,stream_replay"),
        ("82-factory-events","rejected","Binary state for one valid table entry, not analog depth; cannot replace36.","command82_factory_replay"),
        ("37-83-84-calibration","rejected","Calibration changes state and exit persists calibration; unsuitable for non-destructive gameplay reading.","accepted_commands; static handler review in investigation document"),
        ("38-85-settings","rejected","Read saved settings/calibration, not current multi-key depth.","accepted_commands; static handler review in investigation document"),
        ("HID-GET_REPORT","rejected","Reviewed1.09 handler does not configure a response for tested input/output/feature requests.","hid_get_report_replay (1.09 only)")]]
    report["selection"]=dict(candidate="36-simulation",
        rationale="Best of reviewed stock paths that actually supplies analog without entering persistent calibration. Limited fallback only, never a claim of normal support.",
        coverage_limits="All256 dispatcher opcodes routed on each image; not all handler payloads/states/peripherals executed. Hidden mechanisms outside reviewed paths remain unknown.")
    # Pin the static analysis narrative too; routing coverage alone does not prove handler semantics.
    add("investigation",Path(__file__).resolve().parents[1]/"docs/current/MAD68_V2_DUAL_REVIEW_2026-09-27.md","static-analysis")
    for c in report["candidates"]:
        if c["id"] in ("37-83-84-calibration","38-85-settings"):
            c["evidence"]=[dict(asset="investigation",locator="Stock-only investigation continued: Additional static findings")]
    result=validate(report,directory)
    with (directory/"assessment.json").open("x",encoding="utf8") as f:json.dump(report,f,indent=2)
    with (directory/"REPORT.md").open("x",encoding="utf8") as f:f.write(render(report,result))
    print(json.dumps(result,indent=2))
    print("Report:",directory/"REPORT.md")
