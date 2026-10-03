"""Summarize a HallJoy perf run (tools/run_perf_profile.ps1).

Local agent research tool. Reads perf-timeline.txt (written by HallJoy with
--halljoy-perf-log) and perf-phases.txt, then prints:
  * startup / pause / resume / exit timelines (operations and providers);
  * per-phase CPU per thread, named by thread start symbol from the linker map;
  * UI-thread tick and canvas frame costs.
"""
import argparse
import bisect
import collections
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MAP = ROOT / 'build' / 'obj' / 'ReleaseCandidate' / 'x64' / 'HallJoy.map'


def load_map(path):
    syms = []
    if not path.exists():
        return [], []
    for line in path.read_text(encoding='latin-1').splitlines():
        m = re.match(r'\s+[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{16})\s+(?:f\s+)?(?:i\s+)?(\S+)?', line)
        if m:
            syms.append((int(m.group(2), 16) - 0x140000000, m.group(1), m.group(3) or ''))
    syms.sort()
    return syms, [s[0] for s in syms]


def symbol(syms, addrs, module, rva):
    if module.lower() != 'halljoy.exe' or not syms:
        return f'{module}+0x{rva:x}'
    i = bisect.bisect_right(addrs, rva) - 1
    if i < 0:
        return f'{module}+0x{rva:x}'
    name = syms[i][1]
    m = re.match(r'\?([^@]+)@', name)
    readable = m.group(1) if m else name
    scope = re.match(r'\?[^@]+@([^@]+)@', name)
    if scope and not scope.group(1).startswith('?'):
        readable = f'{scope.group(1)}::{readable}'
    return f'{readable} [{syms[i][2]}]'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('run')
    ap.add_argument('--map', default=str(DEFAULT_MAP))
    args = ap.parse_args()
    run = Path(args.run)
    syms, addrs = load_map(Path(args.map))

    events, cpu, threads_final = [], collections.defaultdict(list), {}
    starts = {}
    for line in (run / 'perf-timeline.txt').read_text(encoding='utf-8', errors='replace').splitlines():
        p = line.split()
        if not p:
            continue
        if p[0] == 'event':
            events.append((float(p[1]), float(p[2]), int(p[3]), p[4], int(p[5]), p[6] if len(p) > 6 else '-'))
        elif p[0] == 'cpu':
            tid = int(p[2])
            cpu[tid].append((float(p[1]), float(p[3])))
            starts[tid] = (p[4], int(p[5], 16))
        elif p[0] == 'process':
            print(line)
    phases = []
    phase_file = run / 'perf-phases.txt'
    if phase_file.exists():
        for line in phase_file.read_text().splitlines():
            _, label, a, b = line.split()
            phases.append((label, float(a), float(b)))

    ui_tid = next((e[2] for e in events if e[3] == 'main.entry'), None)
    names = {tid: symbol(syms, addrs, *starts[tid]) for tid in starts}
    if ui_tid:
        names[ui_tid] = 'UI thread (wWinMain)'

    print('\n== Startup (ms since process creation)')
    noisy = {'ui.canvas.frame', 'ui.timer_tick', 'ui.analog_preview_tick'}
    resume_end = next((e[0] + e[1] for e in events if e[3] == 'engine.resume'), 1e9)
    for e in sorted(events, key=lambda e: e[0]):
        if e[3] in noisy or e[0] > resume_end + 1:
            continue
        if e[3] in ('native.prepare_routing', 'native.start') and e[1] < 2.0:
            continue
        print(f'  {e[0]:9.1f}  {e[1]:8.1f} ms  {e[3]:<34} {e[5] if e[5] != "-" else ""}')

    print('\n== Engine transitions')
    for e in events:
        if e[3] in ('engine.pause', 'engine.resume'):
            print(f'  {e[3]:<14} at {e[0]:9.1f}  took {e[1]:8.1f} ms')
    print('\n== Slow operations / providers (>= 5 ms)')
    for e in events:
        if (e[3].startswith('op.') or e[3].startswith('native.') or e[3].startswith('backend.') or
                e[3].startswith('shutdown.') or e[3].startswith('app.') or e[3].startswith('ui.create')) and e[1] >= 5:
            print(f'  {e[0]:9.1f}  {e[1]:8.1f} ms  {e[3]:<30} {e[5] if e[5] != "-" else ""}')

    print('\n== UI thread work (count, total ms, mean ms, max ms)')
    for name in sorted(noisy):
        xs = [e[1] for e in events if e[3] == name]
        if xs:
            print(f'  {name:<24} n={len(xs):6} total={sum(xs):8.1f} mean={sum(xs)/len(xs):6.3f} max={max(xs):7.2f}')

    def cpu_at(samples, t):
        # cumulative cpu (ms) at time t by linear interpolation
        if not samples or t <= samples[0][0]:
            return samples[0][1] if samples else 0.0
        for (t0, c0), (t1, c1) in zip(samples, samples[1:]):
            if t0 <= t <= t1:
                return c0 + (c1 - c0) * (t - t0) / max(t1 - t0, 1e-9)
        return samples[-1][1]

    for label, a, b in phases:
        if label in ('window_found', 'close'):
            continue
        rows = []
        for tid, samples in cpu.items():
            if not samples or samples[-1][0] < a or samples[0][0] > b:
                continue
            used = cpu_at(samples, b) - cpu_at(samples, a)
            if used >= 0.5:
                rows.append((used, tid))
        total = sum(r[0] for r in rows)
        span = (b - a) / 1000.0
        print(f'\n== CPU {label}: {total:.0f} ms over {span:.1f} s = {100*total/(b-a):.2f}% of one core')
        for used, tid in sorted(rows, reverse=True)[:12]:
            print(f'  {used:8.1f} ms  {100*used/(b-a):5.2f}%  tid={tid:<6} {names.get(tid, "?")}')


if __name__ == '__main__':
    main()
