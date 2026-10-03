"""Summarize UI-thread work inside a time window of a HallJoy perf timeline.

Local agent research tool. Usage:
  analyze_perf_window.py <perf-timeline.txt> [--from MARK] [--to MARK]
Defaults to the perf curve sweep (perf.curve_sweep.begin .. end).
Prints per-event-name count/total/mean/p95/max, frame pacing, UI-thread busy %.
"""
import argparse
import collections


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('timeline')
    ap.add_argument('--from', dest='start', default='perf.curve_sweep.begin')
    ap.add_argument('--to', dest='end', default='perf.curve_sweep.end')
    args = ap.parse_args()
    events = []
    for line in open(args.timeline, encoding='utf-8', errors='replace'):
        p = line.split()
        if p and p[0] == 'event':
            events.append((float(p[1]), float(p[2]), int(p[3]), p[4], int(p[5]), p[6] if len(p) > 6 else '-'))
    a = next(e[0] for e in events if e[3] == args.start)
    b = next((e[0] for e in events if e[3] == args.end and e[0] > a), max(e[0] for e in events))
    ui = next((e[2] for e in events if e[3] == 'main.entry'), None)
    span = b - a
    print(f'window {a:.0f}..{b:.0f} ms ({span:.0f} ms), UI tid={ui}')
    groups = collections.defaultdict(list)
    for e in events:
        if a <= e[0] <= b:
            key = e[3] if e[3] != 'ui.dispatch' else f'ui.dispatch[{e[5]} msg=0x{e[4]:04x}]'
            groups[key].append(e[1])
    print(f'{"event":<52} {"n":>6} {"total":>8} {"mean":>7} {"p95":>7} {"max":>7}')
    for name, xs in sorted(groups.items(), key=lambda kv: -sum(kv[1])):
        xs.sort()
        print(f'{name:<52} {len(xs):6} {sum(xs):8.1f} {sum(xs)/len(xs):7.3f} {xs[int(len(xs)*0.95)]:7.3f} {xs[-1]:7.2f}')
    frames = sorted(e[0] for e in events if e[3] == 'ui.canvas.frame' and a <= e[0] <= b)
    if len(frames) > 2:
        gaps = sorted(y - x for x, y in zip(frames, frames[1:]))
        print(f'canvas frames: {len(frames)} = {1000*len(frames)/span:.0f}/s; gap median {gaps[len(gaps)//2]:.1f} ms, '
              f'p95 {gaps[int(len(gaps)*0.95)]:.1f}, max {gaps[-1]:.1f}')
    # UI thread busy: union of top-level spans on the UI thread
    spans = sorted((e[0], e[0] + e[1]) for e in events
                   if e[2] == ui and a <= e[0] <= b and e[1] > 0 and
                   (e[3].startswith('ui.dispatch') or e[3] in ('ui.canvas.frame', 'ui.timer_tick', 'ui.analog_preview_tick')))
    busy, cur_s, cur_e = 0.0, None, None
    for s, e in spans:
        if cur_e is None or s > cur_e:
            if cur_e is not None: busy += cur_e - cur_s
            cur_s, cur_e = s, e
        else:
            cur_e = max(cur_e, e)
    if cur_e is not None: busy += cur_e - cur_s
    print(f'UI thread measured busy: {busy:.0f} ms = {100*busy/span:.0f}% of the window')


if __name__ == '__main__':
    main()
