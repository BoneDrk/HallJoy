"""Behavior assessment layered above execution receipts. Never changes support status."""
import argparse
import hashlib
import json
from pathlib import Path

# Unknown properties remain visible. No physical test requirement is imposed:
# static/branch evidence may establish a property, within its explicitly named scope.
CHECKS = ("normal_typing", "simultaneous_keys", "depth_scale", "shallow_input",
          "temporal_filtering", "release", "physical_mapping", "packet_loss",
          "persistent_settings", "normal_exit", "crash_recovery", "configurator_conflict")
STATES = {"established", "restricted", "unknown", "not_applicable"}
LEVELS = {"static", "branch_replay", "host_test", "hardware_feedback"}

def digest(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def validate(report, root):
    if report.get("schema") != 1: raise ValueError("behavior schema must be 1")
    if not report.get("model") or not report.get("scope"): raise ValueError("exact model and scope required")
    execution = report.get("execution", {})
    if execution.get("outcome") not in {"pass", "fail", "inconclusive"}: raise ValueError("execution outcome required")
    for key in ("entry_boundary", "stop_boundary", "mocked", "not_modeled"):
        if not execution.get(key): raise ValueError("emulation boundary missing: " + key)
    assets = report.get("assets", {})
    if not assets: raise ValueError("pinned evidence assets required")
    roles = set()
    for name, asset in assets.items():
        path = (Path(root) / asset["path"]).resolve()
        if not path.is_relative_to(Path(root).resolve()): raise ValueError("asset escapes report directory")
        if not path.is_file() or digest(path) != asset["sha256"]: raise ValueError("stale/missing asset: " + name)
        roles.add(asset.get("role"))
    if not {"producer-code", "raw-result", "firmware-input"} <= roles:
        raise ValueError("firmware bytes, producer and raw result must be pinned")
    findings = report.get("findings", {})
    if set(findings) != set(CHECKS): raise ValueError("complete behavioral matrix required")
    for name, f in findings.items():
        if f.get("state") not in STATES or not f.get("detail"): raise ValueError("missing finding state/detail: " + name)
        evidence = f.get("evidence", [])
        if f["state"] in {"established", "restricted"} and not evidence:
            raise ValueError("assertion without evidence: " + name)
        for e in evidence:
            if e.get("asset") not in assets or e.get("level") not in LEVELS or not e.get("locator"):
                raise ValueError("invalid evidence reference: " + name)
    candidates=report.get("candidates", [])
    ids=[c.get("id") for c in candidates]
    if not ids or any(not x for x in ids) or len(set(ids))!=len(ids):
        raise ValueError("distinct protocol candidates required")
    for c in candidates:
        if c.get("disposition") not in {"limited", "suitable", "rejected", "unresolved"} or not c.get("reason"):
            raise ValueError("candidate disposition/reason missing")
        if not c.get("evidence") or any(e.get("asset") not in assets or not e.get("locator") for e in c["evidence"]):
            raise ValueError("candidate evidence missing")
    selection=report.get("selection", {})
    if not selection.get("rationale") or not selection.get("coverage_limits"):
        raise ValueError("selection rationale and search limits required")
    selected=selection.get("candidate")
    if selected is not None:
        if selected not in ids or candidates[ids.index(selected)]["disposition"] not in {"limited", "suitable"}:
            raise ValueError("selected path cannot be rejected/unresolved")
    # A caller-supplied verdict must never override recomputation.
    if "verdict" in report: raise ValueError("verdict is derived, not supplied")
    return assess(report)

def assess(report):
    findings = report["findings"]
    restricted = [k for k in CHECKS if findings[k]["state"] == "restricted"]
    unknown = [k for k in CHECKS if findings[k]["state"] == "unknown"]
    selected = next((c for c in report["candidates"] if c["id"] == report["selection"]["candidate"]), None)
    if report["execution"]["outcome"] != "pass": verdict = "EVIDENCE_INCOMPLETE"
    elif selected is None: verdict = "NO_SELECTED_PATH"
    elif restricted or selected["disposition"] == "limited": verdict = "LIMITED_ONLY"
    elif unknown: verdict = "REVIEW_INCOMPLETE"
    else: verdict = "READY_FOR_REVIEW"
    return dict(verdict=verdict, best_available=report["selection"]["candidate"], restrictions=restricted, unknowns=unknown,
                support_status="UNCHANGED; never inferred from test PASS")

def render(report, result):
    lines = ["# " + report["model"] + " — firmware behavior", "",
             "**Protocol assessment: " + result["verdict"] + "**", "",
             "Execution: " + report["execution"]["outcome"] +
             " — expectations reproduced; this is not a support verdict.", "",
             "Scope: " + report["scope"], "", "## Best reviewed option", "",
             "Selected: " + str(report["selection"]["candidate"]), report["selection"]["rationale"],
             "Search limits: " + report["selection"]["coverage_limits"], ""]
    for c in report["candidates"]:
        lines += ["- **" + c["id"] + " [" + c["disposition"] + "]**: " + c["reason"]]
    lines += ["", "## Restrictions and unknowns", ""]
    for state in ("restricted", "unknown", "established", "not_applicable"):
        for key in CHECKS:
            f = report["findings"][key]
            if f["state"] == state:
                lines += ["- **" + key + " [" + state + "]**: " + f["detail"]]
                for e in f.get("evidence", []):
                    lines += ["  Evidence: " + e["level"] + ", " + e["asset"] + ", " + e["locator"]]
    lines += ["", "## Execution boundaries", ""]
    for k,v in report["execution"].items(): lines += ["- " + k + ": " + str(v)]
    lines += ["", "Evidence references/hashes validate provenance, not the truth of an assertion.",
              "No unknown property is silently accepted; no automatic public status changes.", ""]
    return "\n".join(lines)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("report",type=Path)
    p.add_argument("--require-ready",action="store_true");a=p.parse_args()
    r=json.loads(a.report.read_text(encoding="utf8"));result=validate(r,a.report.parent)
    print(json.dumps(result,indent=2))
    return 2 if a.require_ready and result["verdict"] != "READY_FOR_REVIEW" else 0

if __name__ == "__main__": raise SystemExit(main())
