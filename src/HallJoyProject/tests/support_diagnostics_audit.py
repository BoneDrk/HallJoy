from pathlib import Path
import importlib.util
root=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location("support_analyzer",root/"tools/analyze_support_log.py")
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
log="""snapshot.begin seq=1 source=connected native_sources=1
diagnostics.coverage seq=1 catalog=2 visited=2 rows=2 complete=1
native seq=1 index=0 observation=not_present connected=0
native seq=1 index=1 observation=connected connected=1 age_ms=999999
snapshot.end seq=1 rows=2 complete=1
"""
r=m.analyze(log)["latest"]
assert r["structurally_complete"] and r["telemetry_complete"] and r["summary_consistent"]
assert not m.analyze(log.replace("index=1","index=0"))["latest"]["structurally_complete"]
assert not m.analyze(log.split("snapshot.end")[0])["latest"]["structurally_complete"]
assert not m.analyze(log.replace("observation=not_present","observation=unavailable"))["latest"]["telemetry_complete"]
assert not m.analyze(log.replace("source=connected","source=none_reported"))["latest"]["summary_consistent"]
assert m.analyze("snapshot devices=0 analogue_connected=0")["schema"]==1
assert "Legacy" in m.analyze("snapshot devices=0")["limitation"]
assert m.analyze(log+"logging.queue_dropped value=1")["queue_loss_observed"]
assert not m.analyze(log+"snapshot.begin seq=2 source=unknown")["latest"]["structurally_complete"]
print("SUPPORT_DIAGNOSTICS_ANALYZER=PASS coverage duplication truncation unavailable contradiction idle legacy queue_loss")
