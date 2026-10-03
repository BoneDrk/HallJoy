"""ATK keyboards that use the Hex80 analog protocol (02 96 1C travel buffer).

Static extraction from the official ATK hub (hub-v3 3.2.27); no vendor code is
executed and no HID is read.

Sources in docs/research/atk-family-sources/:
- keymap-<path>.json: official hub layout per model (`index` = [row, col]).
- demo-rs6.json: hub demo data dumped from a real RS6 (PID 0x109C).
- hub-facts.json: PIDs, names, controller and matrix size per model, extracted
  from the pinned hub bundle with `--extract-facts` (raw bundle kept in .local).

Matrix slot = row * hub matrix cols, where cols comes from the hub device
filter (`custom.row/col`). This reproduces the hardware-proven Hex80 map
(6x17). Key identities come from, in order of preference: the RS6 device dump,
a keymap whose factory defaults agree with its geometry, or (75% boards, whose
hub defaults are a shifted 68% template) the standard ANSI/ISO position of the
key in the official geometry, with every special width asserted.

Default mode checks generated files byte for byte; `--write` replaces them.
"""
import argparse
import hashlib
import json
import re
import sys
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path

import layout_import as common
import layout_pipeline as pipeline

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'docs/research/atk-family-sources'
REPORTS = ROOT / 'docs/research/atk-family-layout-reports'
HEADER = ROOT / 'src/HallJoyProject/HallJoy/generated/atk_hex80_family.h'
HUB = 'https://bpcdn.atkgear.com/hub-v3/production/3.2.27/'
HUB_JS = ROOT / '.local/research/atk-family-2026-10-01/index-F-MYsmWR.js'
HUB_JS_SHA = '275ec10d7bf0916fea8f27f6cc6415c3eff6e02e682c8164e286626f0f394256'
LABELS = {hid: label for hid, label in common.CODES.values()}
LABELS.update({0x409: 'Fn', 50: '#', 100: '\\'})

# ---- Facts from the hub bundle -------------------------------------------------

PATHS = ['kb_60', 'atk60_rx', 'kb_63', 'atk63_rx', 'kb_68', 'atk_68', 'atk_68_v4', 'atk68_rx', 'rs6_air',
         'edge75', 'edge75_v2', 'rs7_v2', 'rs7_air_iso', 'rs7_revi', 'rs7', 'atk68_v2_pro']


def extract_facts():
    data = HUB_JS.read_bytes()
    common.require(hashlib.sha256(data).hexdigest() == HUB_JS_SHA, 'Hub bundle changed')
    s = data.decode('utf-8')
    i = s.index('"0x373b":{"0x10b0"')
    j = s.index('},"0x1a81"', i)
    pid_paths = json.loads(s[i + len('"0x373b":'):j + 1])
    # Matrix size per device filter: custom:{...spread} or inline row/col.
    spreads = {}
    for m in re.finditer(r'(?:const |,)([\w$]+)=\{([^{}]{0,400}?\brow:(\d+),col:(\d+)[^{}]{0,400}?)\}', s):
        spreads[m.group(1)] = (int(m.group(3)), int(m.group(4)))
    size = {}
    for m in re.finditer(r'productId:(\d+),usagePage:65376,usage:97,custom:\{([^}]*)\}', s):
        rc = re.search(r'\brow:(\d+),col:(\d+)', m.group(2))
        sp = re.search(r'\.\.\.([\w$]+)', m.group(2))
        if rc:
            size[int(m.group(1))] = (int(rc.group(1)), int(rc.group(2)))
        elif sp and sp.group(1) in spreads:
            size[int(m.group(1))] = spreads[sp.group(1)]
    names = {}
    for m in re.finditer(r'\{(?:name:"([^"]+)",)?productId:(\d+),vendorId:14139,usagePage:65376,usage:97(?:,name:"([^"]+)")?\}', s):
        if m.group(1) or m.group(3):
            names.setdefault(int(m.group(2)), set()).add(m.group(1) or m.group(3))
    providers = {}
    for top, provider in (('$c', 'BITYUAN'), ('v8', 'DUCKBREAD')):
        stack = [top]
        while stack:
            name = stack.pop()
            m = re.search(r'(?:const |let |var |,)' + re.escape(name) + r'=\[', s)
            if not m:  # spread of a non-array value
                continue
            start = m.end() - 1
            depth = 0
            for end in range(start, len(s)):
                depth += {'[': 1, ']': -1}.get(s[end], 0)
                if not depth:
                    break
            body = s[start:end + 1]
            stack += re.findall(r'\.\.\.([\w$]+)', body)
            for pid in re.findall(r'productId:(\d+)', body):
                if int(pid) < 0x2000:  # boot loaders use 0x2xxx
                    providers[int(pid)] = provider
    # BITYUAN parser scales travel by 0.001 mm; DUCKBREAD by 0.01 mm (no 02 96 24).
    common.require('getUint16(this.baseOffset+7+e*5+2)*.001' in s and 'getUint16(this.baseOffset+7+e*5+2)*.01' in s,
                   'Travel units changed')
    models = {}
    for pid, provider in sorted(providers.items()):
        path = pid_paths.get(f'0x{pid:x}')
        if pid in (0x1197, 0x1198, 0x11AE, 0x11B0):
            path = 'rs7_v2'  # RS7V2-Ultra filter group; no device path entry
        if path not in PATHS:
            continue
        entry = models.setdefault(path, dict(provider=provider, pids=[], names=[], matrix=None))
        common.require(entry['provider'] == provider, 'Mixed controller for ' + path)
        entry['pids'].append(pid)
        entry['names'] = sorted(set(entry['names']) | names.get(pid, set()))
        if pid in size:
            common.require(entry['matrix'] in (None, list(size[pid])), 'Matrix differs within ' + path)
            entry['matrix'] = list(size[pid])
    facts = dict(source=HUB + 'static/index-F-MYsmWR.js', sourceSha256=HUB_JS_SHA,
                 travelUnits=dict(BITYUAN='0.001 mm, scale via 02 96 24', DUCKBREAD='0.01 mm, no 02 96 24'),
                 models={k: models[k] for k in PATHS})
    for k, v in facts['models'].items():
        common.require(v['matrix'], 'No matrix size for ' + k)
    with (SRC / 'hub-facts.json').open('xb') as f:
        f.write((json.dumps(facts, indent=1) + '\n').encode())


# ---- Model definitions --------------------------------------------------------

def standard_75(top_extra, row4_end, iso=False):
    """Standard 75% rows in physical order; None = key with no reliable identity."""
    f_row = [41] + list(range(58, 70)) + ([None] if top_extra else []) + [76]
    row1 = [53, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 45, 46, 42, 73]
    if iso:
        row2 = [43, 20, 26, 8, 21, 23, 28, 24, 12, 18, 19, 47, 48, 40, 75]
        row3 = [57, 4, 22, 7, 9, 10, 11, 13, 14, 15, 51, 52, 50, 78]
        row4 = [225, 100, 29, 27, 6, 25, 5, 17, 16, 54, 55, 56, 229, 82]
    else:
        row2 = [43, 20, 26, 8, 21, 23, 28, 24, 12, 18, 19, 47, 48, 49, 75]
        row3 = [57, 4, 22, 7, 9, 10, 11, 13, 14, 15, 51, 52, 40, 78]
        row4 = [225, 29, 27, 6, 25, 5, 17, 16, 54, 55, 56, 229, 82] + ([77] if row4_end else [])
    row5 = [224, 227, 226, 44, 230, 0x409, 80, 81, 79]
    return [f_row, row1, row2, row3, row4, row5]


# Width (units) asserted for keys identified by position (HID: width).
WIDE = {42: 2, 43: 1.5, 57: 1.75, 225: 2, 44: 7}

MODELS = [
    # id, sheet names, keymap, HID source, product token, wide name, variant
    dict(id='edge60', path='kb_60', hid='keymap', product='ATK-EDGE60', name='ATK EDGE 60 HE', model='EDGE 60 HE'),
    dict(id='60rx', path='atk60_rx', hid='keymap', product='ATK-60RX', name='ATK 60 RX', model='60 RX'),
    dict(id='edge63', path='kb_63', hid='keymap', product='ATK-EDGE63', name='ATK EDGE 63 HE', model='EDGE 63 HE'),
    dict(id='rs63air', path='atk63_rx', hid=('same', 'kb_63'), product='ATK-RS63AIR', name='ATK RS63 Air', model='RS63 Air'),
    dict(id='rs6', path='kb_68', hid='demo', product='ATK-RS6', name='ATK RS6 / RS6 Ultra', model='RS6 + RS6 Ultra'),
    dict(id='68v3', path='atk_68', hid='keymap', product='ATK-68V3', name='ATK68 V3 / RS6+ / RS6 Ultra+',
         model='68 V3 + RS6+ + RS6 Ultra+'),
    dict(id='rs6cube', path='atk_68_v4', hid=('same', 'atk_68'), product='ATK-RS6CUBE', name='ATK RS6 Cube', model='RS6 Cube'),
    dict(id='68rx', path='atk68_rx', hid='keymap', product='ATK-68RX', name='ATK68 RX', model='68 RX'),
    dict(id='rs6air', path='rs6_air', hid='keymap', product='ATK-RS6AIR', name='ATK RS6 Air', model='RS6 Air'),
    dict(id='edge75', path='edge75', extra_paths=['edge75_v2'], hid=('template', standard_75(True, True)),
         product='ATK-EDGE75', name='ATK EDGE 75 HE', model='EDGE 75 HE'),
    dict(id='rs7v2', path='rs7_v2', hid=('template', standard_75(True, False)), product='ATK-RS7V2',
         name='ATK RS7 V2 / RS7 Air', model='RS7 V2 + RS7 Air'),
    dict(id='rs7airiso', path='rs7_air_iso', hid=('template', standard_75(True, False, iso=True)), variant='ISO',
         product='ATK-RS7AIR-ISO', name='ATK RS7 Air ISO', model='RS7 Air'),
    dict(id='rs7turbo', path='rs7_revi', hid=('template', standard_75(False, False)), product='ATK-RS7TURBO',
         name='ATK RS7 Turbo', model='RS7 Turbo'),
    dict(id='rs7', path='rs7', hid='keymap', product='ATK-RS7', name='ATK RS7', model='RS7'),
    dict(id='68v2pro', path='atk68_v2_pro', hid='keymap', product='ATK-68V2PRO', name='ATK68 V2 Pro', model='68 V2 Pro'),
]
# DUCKBREAD boards report 0.01 mm and have no travel-info command.
FIXED_TRAVEL = {'rs7': 340, '68v2pro': 330}


def read(name):
    path = SRC / name
    data = path.read_bytes()
    return json.loads(data), dict(path=path.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(data).hexdigest())


def keymap_rows(path):
    return read(f'keymap-{path}.json')[0]['layouts']['keymap']


def default_hid(action):
    common.require(isinstance(action, list) and len(action) == 2, 'Bad default action')
    if action == [82, 33]:
        return 0x409
    common.require(action[0] == 0 and action[1] in LABELS, f'Unknown default action {action}')
    return action[1]


def hid_by_index(spec):
    rows = keymap_rows(spec['path'])
    source = spec['hid']
    if source == 'keymap':
        return {tuple(k['index']): default_hid(k['defaultKey']['0']) for r in rows for k in r}
    if source == 'demo':
        demo = read('demo-rs6.json')[0]
        result = {tuple(k['position']): default_hid(k['defaultKey']) for r in demo['keyActions'][0] for k in r}
        common.require(set(result) == {tuple(k['index']) for r in rows for k in r}, 'Demo positions differ')
        return result
    kind, value = source
    if kind == 'same':
        other = keymap_rows(value)
        common.require([[k['index'] for k in r] for r in rows] == [[k['index'] for k in r] for r in other],
                       'Sibling geometry differs')
        return hid_by_index(dict(path=value, hid='keymap'))
    result = {}
    common.require([len(r) for r in rows] == [len(r) for r in value], f'Template row lengths differ for {spec["path"]}')
    for row, template in zip(rows, value):
        for key, hid in zip(row, template):
            if hid is None:
                continue
            wide = {**WIDE, **({225: 1.25, 40: 1.5} if spec.get('variant') == 'ISO' else {})}
            if hid in wide:
                common.require(abs(float(key.get('w', 1)) - wide[hid]) < 0.01,
                               f'Width check failed for HID {hid} in {spec["path"]}')
            result[tuple(key['index'])] = hid
    return result


def px(value):
    return int((value * Decimal(42) / 50).quantize(Decimal(1), rounding=ROUND_HALF_UP))


def layout(spec, hids):
    """Hex80 hub renderer: 50 px unit, 4 px gaps, rows fitted to one width."""
    rows = keymap_rows(spec['path'])
    value = lambda k, n, d: Decimal(str(k.get(n, d)))
    width = max(sum((value(k, 'w', 1) + value(k, 'ml', 0) + value(k, 'mr', 0)) * 50 for k in row) + 4 * (len(row) - 1)
                for row in rows)
    keys, omitted = [], []
    for row_index, row in enumerate(rows):
        right = sum(value(k, 'mr', 0) for k in row) * 50
        unit = (width - 4 * (len(row) - 1) - right) / sum(value(k, 'w', 1) + value(k, 'ml', 0) for k in row)
        x = Decimal(0)
        for index, k in enumerate(row):
            x += value(k, 'ml', 0) * unit + (4 if index else 0)
            y = Decimal(row_index * 54) + value(k, 'mt', 0)
            w, h = value(k, 'w', 1) * unit, value(k, 'h', 1) * 50
            hid = hids.get(tuple(k['index']))
            if not hid:
                omitted.append(dict(position=k['index'], reason='No reliable key identity; geometry gap retained.'))
            else:
                entry = dict(hid=hid, label=LABELS[hid], x=px(x), y=px(y), w=px(x + w) - px(x), h=px(y + h) - px(y))
                if h > 54:  # ISO Enter: full width on its own row, narrower below
                    entry['notchW'] = px(x + unit / 4) - px(x)
                    entry['notchY'] = px(y + 50) - px(y)
                keys.append(entry)
            x += w + value(k, 'mr', 0) * 50
    # ISO Enter: the lower part starts one gap right of the '#' key below it,
    # because the hub fits each row to the same width separately.
    for enter in (k for k in keys if 'notchW' in k):
        below = [k for k in keys if k['hid'] == 50 and k['y'] > enter['y']]
        if below:
            enter['notchW'] = max(enter['notchW'], below[0]['x'] + below[0]['w'] + 4 - enter['x'])
            common.require(enter['notchW'] < enter['w'] - 20, 'ISO Enter lower part too narrow')
    return keys, omitted


def build():
    facts, facts_src = read('hub-facts.json')
    reports, models = [], []
    for spec in MODELS:
        fact = facts['models'][spec['path']]
        pids = list(fact['pids'])
        for extra in spec.get('extra_paths', []):
            other = facts['models'][extra]
            common.require(other['matrix'] == fact['matrix'] and other['provider'] == fact['provider'], 'Extra path differs')
            common.require([[k['index'] for k in r] for r in keymap_rows(extra)] ==
                           [[k['index'] for k in r] for r in keymap_rows(spec['path'])], 'Extra geometry differs')
            pids += other['pids']
        rows, cols = fact['matrix']
        hids = hid_by_index(spec)
        common.require(len(set(hids.values())) == len(hids), 'Duplicate HID in ' + spec['id'])
        slots = [0] * (rows * cols)
        for (r, c), hid in hids.items():
            common.require(r < rows and c < cols, f'Index outside {rows}x{cols} matrix in ' + spec['id'])
            slots[r * cols + c] = hid
        keys, omitted = layout(spec, hids)
        variant = spec.get('variant', 'ANSI')
        sources = [dict(read(f'keymap-{p}.json')[1], url=f'{HUB}keymaps/{p}.json')
                   for p in [spec['path']] + spec.get('extra_paths', [])]
        if spec['hid'] == 'demo':
            sources.append(dict(read('demo-rs6.json')[1], url=HUB + 'demo/rs6.json'))
        if isinstance(spec['hid'], tuple) and spec['hid'][0] == 'same':
            sources.append(dict(read(f'keymap-{spec["hid"][1]}.json')[1], url=f'{HUB}keymaps/{spec["hid"][1]}.json'))
        sources.append(dict(facts_src, url=facts['source']))
        method = {'keymap': 'hub factory defaults (consistent with geometry)', 'demo': 'RS6 device dump'}.get(
            spec['hid'] if isinstance(spec['hid'], str) else '',
            'sibling keymap with identical geometry' if isinstance(spec['hid'], tuple) and spec['hid'][0] == 'same'
            else 'standard position in the official geometry (hub defaults are a shifted 68% template)')
        report = dict(schema=1, id=f'atk_{spec["id"]}_{variant.lower()}', brand='ATK', model=spec['model'],
                      variant=variant, name=f'ATK {spec["model"]} {variant}', status='ready', unresolved=[], keys=keys,
                      omitted=omitted, sources=sources, identity=dict(protocol='hex80', products=[spec['product']]),
                      notes=[f'{fact["provider"]} controller, matrix {rows}x{cols} from the hub device filter.',
                             f'Key identities: {method}.',
                             'Implemented; awaiting hardware testing. No device was used.'])
        pipeline.validate_report(report)
        reports.append(report)
        models.append(dict(spec=spec, pids=sorted(set(pids)), slots=slots, provider=fact['provider']))
    return merge_identical(reports), models


def merge_identical(reports):
    """One preset per distinct geometry (layout catalog rule); every model keeps its own identity token."""
    groups = {}
    for r in reports:
        signature = (r['variant'], json.dumps(sorted(r['keys'], key=lambda k: (k['y'], k['x'])), sort_keys=True))
        groups.setdefault(signature, []).append(r)
    merged = []
    for members in groups.values():
        first = dict(members[0])
        if len(members) > 1:
            first['model'] = ' + '.join(m['model'] for m in members)
            first['name'] = f'ATK {first["model"]} {first["variant"]}'
            first['identity'] = dict(protocol='hex80', products=[p for m in members for p in m['identity']['products']])
            seen, sources = set(), []
            for m in members:
                for s in m['sources']:
                    if s['path'] not in seen:
                        seen.add(s['path'])
                        sources.append(s)
            first['sources'] = sources
            first['notes'] = list(dict.fromkeys(n for m in members for n in m['notes']))
            first['notes'].insert(0, 'Identical geometry and key identities: ' + ', '.join(m['model'] for m in members) + '.')
        pipeline.validate_report(first)
        merged.append(first)
    return merged


def header(models):
    out = ['// Generated by tools/build_atk_hex80_family.py. Do not edit.',
           '// ATK keyboards on the Hex80 protocol; implemented, awaiting hardware testing.',
           '#pragma once', '', 'namespace hex80', '{']
    names = []
    for m in models:
        spec = m['spec']
        sym = 'kAtk' + ''.join(part.capitalize() for part in re.split('[^a-z0-9]+', spec['id']))
        names.append(sym + 'Model')
        out.append(f'// {spec["name"]} ({m["provider"]}); keymap {spec["path"]}.')
        out.append(f'inline constexpr std::array<std::uint16_t, {len(m["pids"])}> {sym}Pids{{{{ '
                   + ', '.join(f'0x{p:04X}' for p in m['pids']) + ' }};')
        values = ', '.join(f'0x{h:x}' if h else '0' for h in m['slots'])
        out.append(f'inline constexpr std::array<std::uint16_t, {len(m["slots"])}> {sym}SlotToHid{{{{ {values} }}}};')
        fixed = FIXED_TRAVEL.get(spec['id'], 0)
        deadzone = 3 if fixed else 8
        out.append(f'inline constexpr Model {sym}Model{{ L"{spec["name"]}", "{spec["product"]}", {sym}Pids.data(), '
                   f'{sym}Pids.size(), {len(m["slots"])}, {sym}SlotToHid.data(), {fixed}, {deadzone}, false }};')
        out.append('')
    out.append(f'inline constexpr std::array<const Model*, {len(names)}> kAtkFamilyModels{{{{')
    out += [f'    &{n},' for n in names]
    out += ['}};', '', '} // namespace hex80', '']
    return '\n'.join(out).encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--extract-facts', action='store_true')
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    if args.extract_facts:
        extract_facts()
    reports, models = build()
    outputs = {REPORTS / (r['id'] + '.json'): (json.dumps(r, indent=2) + '\n').encode() for r in reports}
    outputs[HEADER] = header(models)
    for path, data in outputs.items():
        if args.write:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        else:
            common.require(path.exists() and path.read_bytes() == data, 'Generated output differs: ' + path.name)
    for m in models:
        print(f'{m["spec"]["name"]}: pids={len(m["pids"])} slots={len(m["slots"])} {m["provider"]}')
    for r in reports:
        print(f'preset {r["name"]}: keys={len(r["keys"])} omitted={len(r["omitted"])} products={len(r["identity"]["products"])}')
    print(f'ATK_HEX80_FAMILY=PASS models={len(models)} presets={len(reports)}')


if __name__ == '__main__':
    sys.exit(main())
