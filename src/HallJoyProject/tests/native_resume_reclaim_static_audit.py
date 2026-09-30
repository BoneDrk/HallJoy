#!/usr/bin/env python3
"""Resume after Pause must re-claim native keyboards.

Every engine generation (startup and each Resume) calls NativeAnalogBackends_Reset,
which clears all routing claims, and then asks each provider to prepare again.
A provider that caches "already prepared" and returns early without re-checking
its claim leaves the resumed worker with no routed interface: the keyboard
silently stops working and HallJoy reports an unsupported keyboard.
A re-proof that obeys the worker stop flag left set by Pause fails the same way.

Rule: a prepare/routing function that returns early on a prepared flag must first
verify that its claim still exists (IsClaimedBy, or a routed-only enumeration).
"""
from pathlib import Path
import re
import sys

HALL = Path(__file__).resolve().parents[1] / "HallJoy"
registry = (HALL / "native_analog_backend_registry.cpp").read_text(encoding="utf-8")
app = (HALL / "app.cpp").read_text(encoding="utf-8")

failures = []
if "NativeAnalogRouting_Reset();" not in registry:
    failures.append("registry reset no longer clears routing claims (update this audit)")
enumerate_fresh = app[app.index("static bool EngineRuntimeEnumerateFresh"):]
enumerate_fresh = enumerate_fresh[:enumerate_fresh.index("\n}\n")]
if "NativeAnalogBackends_Reset()" not in enumerate_fresh or "NativeAnalogBackends_PrepareRouting()" not in enumerate_fresh:
    failures.append("each generation must reset claims and prepare providers again")

# Prepare-style function heads whose body starts with an early return on a flag.
HEAD = re.compile(r"^bool (\w*Prepare\w*)\(\)\s*\{", re.M)
LATCH = re.compile(r"if\s*\(\s*g_\w*[Pp]repared\w*\.load\([^)]*\)\s*\)")
RECHECK = ("IsClaimedBy", "EnumerateCandidates(true)", "Enumerate(true,")
checked = 0
for path in sorted(list(HALL.glob("*.cpp")) + list(HALL.glob("*.inc"))):
    text = path.read_text(encoding="utf-8", errors="replace")
    for head in HEAD.finditer(text):
        body = text[head.end():head.end() + 1400]
        first = body.lstrip()[:220]
        latch = LATCH.search(first)
        if not latch:
            continue
        checked += 1
        guarded = body[:body.find("\n}\n") if "\n}\n" in body else 900]
        window = guarded[:900]
        if not any(marker in window for marker in RECHECK):
            failures.append(f"{path.name}:{head.group(1)} returns a cached prepare result without re-checking its routing claim")

# Re-proving happens between generations, while the worker stop flag from the
# previous Pause is still set. A probe that obeys that flag fails every time,
# so the keyboard is never claimed again after Resume.
hex80 = (HALL / "hex80_backend.cpp").read_text(encoding="utf-8")
session = hex80[hex80.index("class Session"):hex80.index("enum class ProofStage")]
probe = hex80[hex80.index("bool ProbeCandidate("):hex80.index("void ResetTelemetry()")]
if "g_stop" in session:
    failures.append("hex80 Session I/O reads the worker stop flag; routing probes after Pause would fail")
if "Session session(candidate, nullptr)" not in probe:
    failures.append("hex80 routing probe must not follow the worker stop flag")

if checked < 4:
    failures.append(f"expected at least 4 latched prepare functions to audit, found {checked}")
if failures:
    print("NATIVE_RESUME_RECLAIM_STATIC_AUDIT=FAIL")
    for failure in failures:
        print(" - " + failure)
    sys.exit(1)
print(f"NATIVE_RESUME_RECLAIM_STATIC_AUDIT=PASS latched_prepare_functions={checked}")
