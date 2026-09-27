#!/usr/bin/env python3
"""Read-only structural analysis. Never infer missing analog from silence/zero alone."""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

def fields(line):
    return dict(re.findall(r"([a-z_]+)=([^ ]+)", line))

def number(data, key):
    try:
        return int(data[key])
    except (KeyError, ValueError):
        return None

def analyze(text):
    snapshots=[]
    current=None
    events=[]
    loss=False
    for line in text.splitlines():
        if "logging.queue_dropped" in line:
            loss=True
        if "mix87." in line:
            events.append(line)
        if "snapshot.begin " in line:
            current={"begin":fields(line),"coverage":{},"providers":[],"end":{}}
            snapshots.append(current)
        elif current is not None:
            data=fields(line)
            if data.get("seq")!=current["begin"].get("seq"):
                continue
            if "diagnostics.coverage " in line: current["coverage"]=data
            elif re.search(r"(?:^| )native seq=",line): current["providers"].append(data)
            elif "snapshot.end " in line: current["end"]=data; current=None
    for s in snapshots:
        c=s["coverage"]; rows=s["providers"]; end=s["end"]
        expected=number(c,"catalog")
        indices=[number(r,"index") for r in rows]
        s["structurally_complete"]=(expected is not None and expected>=0 and
            number(c,"visited")==expected and number(c,"rows")==expected and
            number(end,"rows")==expected and len(rows)==expected and
            set(indices)==set(range(expected)))
        s["telemetry_complete"]=(s["structurally_complete"] and c.get("complete")=="1"
            and end.get("complete")=="1" and all(r.get("observation")!="unavailable" for r in rows))
        connected=sum(r.get("connected")=="1" for r in rows)
        s["summary_consistent"]=(number(s["begin"],"native_sources")==connected and
            (connected==0 or s["begin"].get("source")=="connected"))
    latest=snapshots[-1] if snapshots else None
    result={"schema":2 if snapshots else 1,"snapshots":len(snapshots),
        "latest":latest,"queue_loss_observed":loss,
        "interpretation": "Provider observations are sampled separately. Counters retain provider-defined scope. Silence and zero counts alone do not establish failure."}
    if not snapshots:
        result["limitation"]="Legacy log has no verifiable catalog coverage or snapshot completeness; do not conclude that analog failed from its summary."
    elif not latest["structurally_complete"]:
        result["limitation"]="Latest snapshot is incomplete; do not treat missing rows as absent devices."
    elif not latest["telemetry_complete"]:
        result["limitation"]="Some provider telemetry is unavailable; absence is not established."
    elif not latest["summary_consistent"]:
        result["limitation"]="Summary contradicts provider observations; investigate diagnostics before interpreting device failure."
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log",type=Path)
    args=parser.parse_args()
    print(json.dumps(analyze(args.log.read_text(encoding="utf-8-sig",errors="replace")),ensure_ascii=True,indent=2))
if __name__=="__main__":main()
