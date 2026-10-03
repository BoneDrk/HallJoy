"""Aggregate UI-thread stack samples from a HallJoy perf timeline.

Local agent research tool. Requires a run with --halljoy-perf-stacks.
Usage: analyze_perf_stacks.py <perf-timeline.txt> [--from MARK] [--to MARK] [--map HallJoy.map]
Prints the share of samples per leaf module, the hottest HallJoy functions as
leaf (self) and inclusive (anywhere on the stack), within the marked window.
"""
import argparse
import bisect
import collections
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load_map(path):
    syms = []
    for line in Path(path).read_text(encoding='latin-1').splitlines():
        m = re.match(r'\s+[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{16})\s+(?:f\s+)?(?:i\s+)?(\S+)?', line)
        if m:
            syms.append((int(m.group(2), 16) - 0x140000000, m.group(1), m.group(3) or ''))
    syms.sort()
    return syms, [s[0] for s in syms]


def demangle(name):
    m = re.match(r'\?([^@]+)@(?:([^@?][^@]*)@)?', name)
    if not m:
        return name
    return f'{m.group(2)}::{m.group(1)}' if m.group(2) else m.group(1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('timeline')
    ap.add_argument('--from', dest='start', default='perf.curve_sweep.begin')
    ap.add_argument('--to', dest='end', default='perf.curve_sweep.end')
    ap.add_argument('--map', default=str(ROOT / 'build/obj/ReleaseCandidate/x64/HallJoy.map'))
    args = ap.parse_args()
    syms, addrs = load_map(args.map)

    def name(frame):
        mod, rva = frame.rsplit('+0x', 1)
        if mod.lower() != 'halljoy.exe':
            return mod
        i = bisect.bisect_right(addrs, int(rva, 16)) - 1
        return demangle(syms[i][1]) if i >= 0 else frame

    a = b = None
    stacks = []
    for line in open(args.timeline, encoding='utf-8', errors='replace'):
        p = line.split()
        if not p:
            continue
        if p[0] == 'event' and p[4] == args.start and a is None:
            a = float(p[1])
        elif p[0] == 'event' and p[4] == args.end:
            b = float(p[1])
        elif p[0] == 'stk':
            stacks.append((float(p[1]), p[2:]))
    a = a or 0.0
    b = b or 1e12
    window = [frames for t, frames in stacks if a <= t <= b and frames]
    n = len(window)
    print(f'samples in window: {n} (~{n} ms at 1 kHz)')
    leaf_mod = collections.Counter(f[0].rsplit('+0x', 1)[0] for f in window)
    print('\n== leaf module share')
    for mod, c in leaf_mod.most_common(12):
        print(f'  {100*c/n:5.1f}%  {mod}')
    # leaf HallJoy function, or the first HallJoy caller for samples in other modules
    first_own = collections.Counter()
    inclusive = collections.Counter()
    for frames in window:
        names = [name(f) for f in frames]
        own = [nm for f, nm in zip(frames, names) if f.lower().startswith('halljoy.exe')]
        if own:
            first_own[own[0]] += 1
        for nm in set(own):
            inclusive[nm] += 1
    print('\n== first HallJoy frame (self + callees outside HallJoy)')
    for nm, c in first_own.most_common(20):
        print(f'  {100*c/n:5.1f}%  {nm}')
    print('\n== inclusive HallJoy functions')
    for nm, c in inclusive.most_common(30):
        print(f'  {100*c/n:5.1f}%  {nm}')


if __name__ == '__main__':
    main()
