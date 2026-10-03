"""Layouts for Supported keyboards that had no exact preset (2026-09-30).

Static extraction only: no vendor code is executed and no HID is read.
`--extract` rebuilds the public JSON extractions from raw manufacturer files
kept in `.local` (hash-pinned). The default mode rebuilds the reviewed reports
from those extractions and checks them byte for byte; `--write-new` creates
missing reports exclusively.
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
SOURCES = ROOT / 'docs/research/supported-gap-layout-sources'
REPORTS = ROOT / 'docs/research/supported-gap-layout-reports'
LABELS = {hid: label for hid, label in common.CODES.values()}
LABELS[0x409] = 'Fn'

# Raw manufacturer files (private cache) and where they came from.
RAW = {
    'illumipc': ('.local/research/layout-gaps-2026-09-30/illumipc-SG8994HERGB.json',
                 '1a65f1b40af0e675ca915ffc49dfb47be1fd40234917467f1897eb00ffa40aaa',
                 'https://www.illumipc.com/config/keys/SG8994HERGB.json'),
    'mhub': ('.local/research/layout-gaps-2026-09-30/layout-mix87_i.10532226902f2ad66259.js',
             '4b9a7d34559762c930c6fa24ce93f8356f5b14153776bc862a9f62ceee14f7a6',
             'https://www.mchose.com.cn/cizhou/CZ_SHARED_DATA/layout-mix87_i.10532226902f2ad66259.js'),
    'mhub-jet75': ('.local/research/mchose-mhub-20261002/web/layout-jet75_i.0299ea0625c2bbb38b0d.js',
                   'df0de39bce980550ce11a81d0907e538a45221b5876a9093b980c03533131085',
                   'https://www.mchose.com.cn/cizhou/CZ_SHARED_DATA/layout-jet75_i.0299ea0625c2bbb38b0d.js'),
    'mhub-ace68': ('.local/research/mchose-mhub-20261002/web/layout-ace68_i.344c52158c7dec6b0d6b.js',
                   '34434c634222821ec64a88a598901c0039b5814eeb09c05defb0b3bc12ee89a6',
                   'https://www.mchose.com.cn/cizhou/CZ_SHARED_DATA/layout-ace68_i.344c52158c7dec6b0d6b.js'),
    'mhub-ace60': ('.local/research/mchose-mhub-20261002/web/layout-ace60.bb29f679b61b21be5e1a.js',
                   '4669662fa95a2d8aad4dcac76d3b2184e16529764627fe2fe4611f0bc9820a92',
                   'https://www.mchose.com.cn/cizhou/CZ_SHARED_DATA/layout-ace60.bb29f679b61b21be5e1a.js'),
    'mhub-ace60-nordic': ('.local/research/mchose-mhub-20261002/web/layout-ace60_pro_nordic.48404a6c22fdeaf8294b.js',
                          '004a0c351c5f2b7dbdbe28d17ffb0427664b4f0bdaee1b3061933c8232f265fe',
                          'https://www.mchose.com.cn/cizhou/CZ_SHARED_DATA/layout-ace60_pro_nordic.48404a6c22fdeaf8294b.js'),
    'mhub-ace75': ('.local/research/mchose-mhub-20261002/web/layout-ace75_8k.a60682317cbda548c1e7.js',
                   '913444c6b59b0470e77ad1bf5962684a6b204ae9802816af5d0c036de599491c',
                   'https://www.mchose.com.cn/cizhou/CZ_SHARED_DATA/layout-ace75_8k.a60682317cbda548c1e7.js'),
    'rk68he': ('.local/research/rk-hubx-20261002/rk-app-BqfTKFxh.js',
               'b6144986513c0e91b17f340c9fbd126911d9596132ef466e7b25e760d6586ef8',
               'https://rk.hubx.pro/app-BqfTKFxh.js'),
    'nuphy': ('docs/research/final-layout-sources/nuphy.js',
              '8b8fe5fdb1c13092bd3f38476688e4435b2ffdfcca552ba4b36d5fe149f27fba',
              'https://drive.nuphy.io/static/js/main.23dc78ef.js'),
    'gg': ('.local/research/steelseries-apex-20260925/extracted/asar/index.css',
           'f397494d12bb34151131640cf433efc80a3583eda4a98b55bda745e3a1ee1b5f',
           'https://engine.steelseriescdn.com/SteelSeriesGG120.0.0Setup.exe'
           ' (sha256 88b091f2a316d2bdbaed6cc78437b44fb57ae7252cf13be5a7466e468eb7cb26)'
           ' > resources/app.asar (sha256 4cc19f7ec395083fa5b7ee8d89e681a8bf204134f0c7c3f60fa91cbab1e94ecb)'
           ' > render/index.css'),
}
EXTRACTS = {'illumipc': 'illumipc-SG8994HERGB.json', 'mhub': 'mhub-mix87-iii-keys.json',
            'mhub-jet75': 'mhub-jet75-ii-keys.json', 'mhub-ace68': 'mhub-ace68-keys.json',
            'mhub-ace60': 'mhub-ace60-keys.json', 'mhub-ace60-nordic': 'mhub-ace60-pro-nordic-keys.json',
            'mhub-ace75': 'mhub-ace75-8k-keys.json', 'rk68he': 'rk-rk68he-keys.json',
            'nuphy': 'nuphy-field75-he-keys.json', 'gg': 'steelseries-gg-apex-pro-us.json'}


def roundpx(value):
    return int(Decimal(str(value)).quantize(Decimal(1), rounding=ROUND_HALF_UP))


def raw_text(name):
    path, sha, _ = RAW[name]
    data = (ROOT / path).read_bytes()
    common.require(hashlib.sha256(data).hexdigest() == sha, 'Raw source changed: ' + path)
    return data.decode('utf-8')


def literal(text, marker):
    start = text.index(marker) + len(marker)
    opening = text[start]
    closing = {'{': '}', '[': ']'}[opening]
    depth, quoted, escaped = 0, None, False
    for i in range(start, len(text)):
        c = text[i]
        if quoted:
            if escaped:
                escaped = False
            elif c == '\\':
                escaped = True
            elif c == quoted:
                quoted = None
        elif c in ('"', "'"):
            quoted = c
        elif c == opening:
            depth += 1
        elif c == closing:
            depth -= 1
            if not depth:
                return text[start:i + 1]
    raise ValueError('Unterminated literal: ' + marker)


def js_objects(raw):
    """Minimal reader for the flat literal arrays used here (no code execution)."""
    raw = raw.replace('!0', 'true').replace('!1', 'false')
    raw = re.sub(r'([{,])\s*([A-Za-z_$][\w$]*)\s*:', r'\1"\2":', raw)
    raw = re.sub(r'(?<=[:\[,])\s*\.(\d)', r'0.\1', raw)
    raw = re.sub(r'(?<=[:\[,])-\.(\d)', r'-0.\1', raw)
    return json.loads(raw)


NUMBER = r'(-?(?:\d+\.?\d*|\.\d+)(?:e-?\d+)?)'


def regex_keys(raw):
    """Key objects from a literal whose icon fields are JSX expressions (not
    JSON-readable). Reads only x, y, w, h, code, name and type of flat objects."""
    keys = []
    for m in re.finditer(r'\{x:' + NUMBER + r',y:' + NUMBER + r'([^{}]*?)\}', raw):
        body = m.group(3)
        field = lambda k: re.search(r'\b' + k + r':' + NUMBER, body)
        code = field('code')
        if not code:
            continue
        key = dict(x=float(m.group(1)), y=float(m.group(2)))
        for k in ('w', 'h'):
            if field(k):
                key[k] = float(field(k).group(1))
        key['code'] = int(float(code.group(1)))
        for k in ('name', 'type'):
            s = re.search(r'\b' + k + r':"([^"]*)"', body)
            if s:
                key[k] = s.group(1)
        keys.append(key)
    return keys


def flat_objects(raw):
    """Quote-aware split of a literal into its top-level {...} objects; returns
    simple key:value fields (strings unescaped, numbers as text)."""
    keys, depth, quoted, cur = [], 0, False, ''
    for i, c in enumerate(raw):
        if c == '"' and raw[i - 1] != '\\':
            quoted = not quoted
        if not quoted and c == '{':
            depth += 1
            if depth == 1:
                cur = ''
                continue
        if not quoted and c == '}':
            depth -= 1
            if depth == 0:
                fields = {}
                for m in re.finditer(r'(\w+):("((?:[^"\\]|\\.)*)"|[^,]+)', cur):
                    fields[m.group(1)] = m.group(3) if m.group(3) is not None else m.group(2)
                keys.append(fields)
                continue
        if depth >= 1:
            cur += c
    return keys


def rk_hid(value):
    """BY/IPI factory keycode: 0x0D000000 is Fn, high bits are modifiers."""
    value = int(value, 16)
    if value == 0x0D000000:
        return 0x409
    if value >= 0x10000:
        return 223 + (value >> 16).bit_length()
    return value


def extract():
    """Write public extractions from the pinned raw files (exclusive create)."""
    out = {}
    out['illumipc'] = raw_text('illumipc').encode('utf-8')
    text = raw_text('mhub')
    keys = js_objects(literal(text, 'keys:'))
    out['mhub'] = dict(source=RAW['mhub'][2], sourceSha256=RAW['mhub'][1],
                       note='Mix87 III (3837:300D) loads chunk 9677, which re-exports this Mix87 layout module 654.',
                       keyScalePx=42, keys=[{k: key[k] for k in ('x', 'y', 'w', 'h', 'code', 'name', 'type') if k in key}
                                             for key in keys])
    text = raw_text('mhub-jet75')
    keys = js_objects(literal(text[text.index('99605:'):], 'keys:'))
    out['mhub-jet75'] = dict(source=RAW['mhub-jet75'][2], sourceSha256=RAW['mhub-jet75'][1],
                             note='Jet75 II (41E4:211A) loads chunk 232, which re-exports this Jet75 layout module 99605.',
                             keyScalePx=42, keys=[{k: key[k] for k in ('x', 'y', 'w', 'h', 'code', 'name', 'type') if k in key}
                                                   for key in keys])
    text = raw_text('mhub-ace68')
    keys = js_objects(literal(text[text.index('76340:'):], 'keys:'))
    out['mhub-ace68'] = dict(source=RAW['mhub-ace68'][2], sourceSha256=RAW['mhub-ace68'][1],
                             note='Ace68-II (41E4:2116, catalog "Ace 68 Pro") loads chunk 3053, whose module 91684 re-exports this Ace68 layout module 76340.',
                             keyScalePx=42, keys=[{k: key[k] for k in ('x', 'y', 'w', 'h', 'code', 'name', 'type') if k in key}
                                                   for key in keys])
    for name, module, note in (
            ('mhub-ace60', '52940', 'Ace 60 Pro (41E4:2103) and Ace 60X I/II (41E4:2126, 41E4:2112): their chunks re-export '
             'Ace60 module 52940 or Ace60X module 79628, whose keys are identical after translation.'),
            ('mhub-ace60-nordic', '46612', 'Ace60 Pro Nordic (3837:3002) loads this ISO layout module 46612.')):
        text = raw_text(name)
        keys = js_objects(literal(text[text.index(module + ':'):], 'keys:'))
        out[name] = dict(source=RAW[name][2], sourceSha256=RAW[name][1], note=note, keyScalePx=42,
                         keys=[{k: key[k] for k in ('x', 'y', 'w', 'h', 'code', 'name', 'type') if k in key} for key in keys])
    text = raw_text('mhub-ace75')
    keys = regex_keys(literal(text[text.index('37685:'):], 'keys:'))
    out['mhub-ace75'] = dict(source=RAW['mhub-ace75'][2], sourceSha256=RAW['mhub-ace75'][1],
                             note='Ace 75 8K (3837:303C) layout module 37685; icon fields are JSX and are not extracted. '
                                  'Entries of type function-key are the knob/screen controls.',
                             keyScalePx=42, keys=keys)
    text = raw_text('rk68he')
    layouts = {}
    for name, marker in (('ANSI', 'wrt="RK_68_HE"'), ('UK', 'Hrt="RK_68_HE_UK"')):
        # Layer 0 only: the first inner array of keyboard:[[...],...] (quote-aware).
        start = text.index('keyboard:[[', text.index(marker)) + len('keyboard:[')
        depth, quoted = 0, False
        for end in range(start, len(text)):
            c = text[end]
            if c == '"' and text[end - 1] != '\\':
                quoted = not quoted
            elif not quoted and c == '[':
                depth += 1
            elif not quoted and c == ']':
                depth -= 1
                if depth == 0:
                    break
        keys = flat_objects(text[start:end + 1])
        seen, layer0 = set(), []
        for k in keys:
            if k.get('pos') in (None, 'null') or not k.get('value'):
                continue
            common.require(int(k['pos']) not in seen, 'RK68 HE duplicate physical ID')
            seen.add(int(k['pos']))
            layer0.append(dict(name=k.get('name'), id=int(k['pos']), value=k['value'], x=float(k['x']), y=float(k['y']),
                               w=float(k['width']), h=float(k['height'])))
        layouts[name] = layer0
    out['rk68he'] = dict(source=RAW['rk68he'][2], sourceSha256=RAW['rk68he'][1],
                         note='Official RK web driver (BY platform). RK68 HE 372E:10BF UUID 0x110000000002 uses RK_68_HE; '
                              '372E:10C0 UUID 0x11000000003C uses RK_68_HE_UK. Layer 0, WIN. id = BY physical key ID; '
                              'value = factory keycode (0x0D000000 Fn, modifier bits). Pixel geometry of the driver image.',
                         layouts=layouts)
    text = raw_text('nuphy')
    module = text[text.index('97075:function'):]
    raw = re.sub(r's\.A\.(KC_\w+)', r'"\1"', literal(module, ',_='))
    keys = js_objects(raw)
    out['nuphy'] = dict(source=RAW['nuphy'][2], sourceSha256=RAW['nuphy'][1],
                        note='Module 97075 export As (local _) is Field75HE keys; module 69731 excludes indexes 30,46,61,76,87,88,89,90 from HE settings.',
                        excludedIndexes=[30, 46, 61, 76, 87, 88, 89, 90],
                        keys=[{k: key[k] for k in ('keyCode', 'x', 'y', 'w', 'h') if k in key} for key in keys])
    css = raw_text('gg')
    rules = {}
    for m in re.finditer(r'([^{}]*)\{([^}]*)\}', css):
        for selector in m.group(1).split(','):
            selector = selector.strip()
            code = re.search(r'\.code(\d+)\b', selector)
            region = re.search(r'region-(\d+)', selector)
            if not selector.startswith('#DeviceConfig.apex_pro ') or not code or (region and region[1] != '1'):
                continue
            props = dict(re.findall(r'([a-z\-]+)\s*:\s*([^;]+);', m.group(2)))
            rules.setdefault(code[1], {}).update({k: props[k].strip() for k in ('left', 'top', 'width', 'height', 'display') if k in props})
    out['gg'] = dict(source=RAW['gg'][2], sourceSha256=RAW['gg'][1],
                     note='#DeviceConfig.apex_pro .code<HID> rules, base plus region-1 (US) overrides. Default zone 26x28 px.',
                     zone=dict(width=26, height=28), rules={k: rules[k] for k in sorted(rules, key=int)})
    SOURCES.mkdir(parents=True, exist_ok=True)
    for name, data in out.items():
        if not isinstance(data, bytes):
            data = (json.dumps(data, indent=1, ensure_ascii=True) + '\n').encode()
        path = SOURCES / EXTRACTS[name]
        if path.exists():  # Never overwrite: an existing extraction must be identical.
            common.require(path.read_bytes() == data, 'Extraction differs: ' + path.name)
            continue
        with path.open('xb') as f:
            f.write(data)


def source(name):
    path = SOURCES / EXTRACTS[name]
    data = path.read_bytes()
    return json.loads(data), dict(path=path.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(data).hexdigest(),
                                  url=RAW[name][2])


def key(hid, x, y, right, bottom):
    return dict(hid=hid, label=LABELS[hid], x=x, y=y, w=right - x - 4, h=bottom - y - 4)


def report(brand, model, keys, src, protocol, products, notes, omitted, variant='ANSI'):
    minx = min(k['x'] for k in keys)
    miny = min(k['y'] for k in keys)
    for k in keys:
        k['x'] -= minx
        k['y'] -= miny
    symbol = re.sub('[^a-z0-9]+', '_', f'{brand} {model} {variant}'.lower()).strip('_')
    result = dict(schema=1, id=symbol, brand=brand, model=model, variant=variant, name=f'{brand} {model} {variant}',
                  status='ready', unresolved=[], keys=keys, omitted=omitted, sources=[src],
                  identity=dict(protocol=protocol, products=products), notes=notes)
    if not products:
        result['identity']['requiresVerifiedSession'] = True
        result['autoSelection'] = 'Disabled: the plugin session does not prove the physical variant.'
    pipeline.validate_report(result)
    return result


def prepare():
    reports = []
    # AJAZZ AK820 MAX HE wired RGB: official illumipc key file (35 px keys, 38 px pitch).
    data, src = source('illumipc')
    s = 46 / 38
    keys, omitted = [], []
    for k in data['keys']:
        if k['code'] == 'RotaryKnob':
            omitted.append(dict(code='RotaryKnob', reason='Rotary encoder, not a key'))
            continue
        hid = 0x409 if k['code'] == 'KeyFn' else int(k['hidCode'], 16)
        x, y, w, h = (float(k[f]) for f in ('x', 'y', 'width', 'height'))
        keys.append(key(hid, roundpx(x * s), roundpx(y * s), roundpx((x + w + 3) * s), roundpx((y + h + 3) * s)))
    common.require(len(keys) == 82, 'AK820 key count')
    reports.append(report('AJAZZ', 'AK820 MAX HE', keys, src, 'ajazz-m484', ['SG8994HERGB'],
                          ['Official illumipc geometry for SG8994HERGB (identical to SG8994HE). Supported scope: wired RGB SG8994HERGB V1.13.17.'],
                          omitted))
    # MCHOSE Mix 87 III: M HUB layout, units of the 42 px key with a 46 px pitch.
    data, src = source('mhub')
    keys, omitted = [], []
    for k in data['keys']:
        if k.get('type') == 'function-key':
            omitted.append(dict(code=k['name'], reason='Top-right function button (G1/RT/Light), no analog channel'))
            continue
        hid = 0x409 if k['code'] == 255 else k['code']
        x, y = k['x'] * 42, k['y'] * 42
        keys.append(key(hid, roundpx(x), roundpx(y), roundpx(x + k.get('w', 1) * 42) + 4, roundpx(y + k.get('h', 1) * 42) + 4))
    common.require(len(keys) == 87, 'Mix87 key count')
    reports.append(report('MCHOSE', 'Mix 87 III', keys, src, 'mchose-mix87', ['MIX87III-3837-300D', 'MIX87I-41E4-2122'],
                          ['Official M HUB geometry; Mix87 III reuses the Mix87 layout module. Fn is display-only.',
                           'Mix 87 I (41E4:2122) loads the same module; its firmware A0 slots equal these 86 keys + Fn.'],
                          omitted))
    # MCHOSE Jet 75 II: same M HUB units as Mix87 (42 px key, 46 px pitch).
    data, src = source('mhub-jet75')
    keys = []
    for k in data['keys']:
        hid = 0x409 if k['code'] == 255 else k['code']
        x, y = k['x'] * 42, k['y'] * 42
        keys.append(key(hid, roundpx(x), roundpx(y), roundpx(x + k.get('w', 1) * 42) + 4, roundpx(y + k.get('h', 1) * 42) + 4))
    common.require(len(keys) == 80, 'Jet75 key count')
    reports.append(report('MCHOSE', 'Jet 75 II', keys, src, 'mchose-jet75',
                          ['JET75II-41E4-211A', 'ZERO75X-41E4-211C', 'JET75I-41E4-2118'],
                          ['Official M HUB geometry; Jet75 II and III reuse the Jet75 layout module. Fn is display-only.',
                           'Zero75X (41E4:211C) and Jet 75 I (41E4:2118) load the same module; their firmware A0 slots equal these keys.'],
                          []))
    # MCHOSE Ace 68 (Ace68-II, 41E4:2116): same M HUB units as Jet75.
    data, src = source('mhub-ace68')
    keys = []
    for k in data['keys']:
        hid = 0x409 if k['code'] == 255 else k['code']
        x, y = k['x'] * 42, k['y'] * 42
        keys.append(key(hid, roundpx(x), roundpx(y), roundpx(x + k.get('w', 1) * 42) + 4, roundpx(y + k.get('h', 1) * 42) + 4))
    common.require(len(keys) == 68, 'Ace68 key count')
    # Firmware 1.21 descriptor table (A0 slots): 67 HID keys + Fn.
    firmware = set(range(4, 50)) | {51, 52, 54, 55, 56, 57, 73, 75, 76, 78, 79, 80, 81, 82} | set(range(0xE0, 0xE7))
    common.require({k['hid'] for k in keys} == firmware | {0x409}, 'Ace68 layout differs from firmware slots')
    reports.append(report('MCHOSE', 'Ace 68', keys, src, 'mchose-ace68',
                          ['ACE68II-41E4-2116', 'ACE68I-41E4-2114', 'ACE68AIRII-41E4-2120', 'ACE68III-3837-3003',
                           'ACE68AIRIII-41E4-2132', 'ACE68AIR2-3837-300A', 'ACE68V2III-3837-3024', 'ACE68TURBO8K-3837-3028'],
                          ['Official M HUB geometry; the 41E4:2116 (USB "Ace68-II") entry re-exports the Ace68 layout module. Fn is display-only.',
                           'Ace 68 I (41E4:2114) loads the same module; Ace 68 Air II (41E4:2120) module 96178 is identical after translation. Both firmware key sets equal these keys.',
                           'ARM boards Ace 68 III, Air III, Air 2, V2 III and Turbo 8K: official layouts identical after translation; firmware A0 slots equal these keys.'],
                          []))
    # MCHOSE Ace 60 Pro / Ace 60X I / Ace 60X II (shared 60% geometry) and Ace 60 Pro Nordic (ISO).
    ace60 = set(range(4, 50)) | {51, 52, 54, 55, 56, 57, 101} | set(range(0xE0, 0xE7))
    for name, model, variant, products, count, firmware, note in (
            ('mhub-ace60', 'Ace 60', 'ANSI', ['ACE60PRO-41E4-2103', 'ACE60XI-41E4-2126', 'ACE60XII-41E4-2112'], 61, ace60,
             'Official M HUB geometry shared by Ace 60 Pro and Ace 60X I/II; firmware A0 slots equal these 60 keys + Fn. Fn is display-only.'),
            ('mhub-ace60-nordic', 'Ace 60 Pro Nordic', 'ISO', ['ACE60PRONORDIC-3837-3002'], 62, ace60 | {100},
             'Official M HUB Nordic ISO geometry; firmware 1.07 A0 slots equal these 61 keys + Fn. Fn is display-only.')):
        data, src = source(name)
        keys = []
        for k in data['keys']:
            hid = 0x409 if k['code'] == 255 else k['code']
            x, y = k['x'] * 42, k['y'] * 42
            keys.append(key(hid, roundpx(x), roundpx(y), roundpx(x + k.get('w', 1) * 42) + 4, roundpx(y + k.get('h', 1) * 42) + 4))
        common.require(len(keys) == count, model + ' key count')
        common.require({k['hid'] for k in keys} == firmware | {0x409}, model + ' layout differs from firmware slots')
        if variant == 'ISO':
            # M HUB draws ISO Enter as one tall rectangle over the key on its
            # lower left; HallJoy uses a notched Enter (as for ATK ISO boards).
            enter = next(k for k in keys if k['hid'] == 40)
            below = [k for k in keys if k['y'] > enter['y'] and k['y'] < enter['y'] + enter['h']
                     and k['x'] < enter['x'] < k['x'] + k['w'] + 4]
            common.require(len(below) == 1, model + ' ISO Enter neighbour')
            enter['notchY'] = below[0]['y'] - enter['y']
            enter['notchW'] = below[0]['x'] + below[0]['w'] + 4 - enter['x']
            common.require(0 < enter['notchW'] < enter['w'] - 20, model + ' ISO Enter lower part too narrow')
        reports.append(report('MCHOSE', model, keys, src, 'mchose-ace60', products, [note], [], variant))
    # MCHOSE Ace 75 8K (ARM, Mix87 III design): own 80-key geometry.
    data, src = source('mhub-ace75')
    keys, omitted = [], []
    for k in data['keys']:
        if k.get('type') == 'function-key':
            omitted.append(dict(code=k.get('name', str(k['code'])), reason='Knob/screen control, no analog channel'))
            continue
        hid = 0x409 if k['code'] == 255 else k['code']
        x, y = k['x'] * 42, k['y'] * 42
        keys.append(key(hid, roundpx(x), roundpx(y), roundpx(x + k.get('w', 1) * 42) + 4, roundpx(y + k.get('h', 1) * 42) + 4))
    common.require(len(keys) == 81, 'Ace 75 8K key count')
    firmware = set(range(4, 50)) | set(range(51, 70)) | set(range(74, 83)) | set(range(0xE0, 0xE6))
    common.require({k['hid'] for k in keys} == firmware | {0x409}, 'Ace 75 8K layout differs from firmware slots')
    reports.append(report('MCHOSE', 'Ace 75 8K', keys, src, 'mchose-ace75', ['ACE758K-3837-303C'],
                          ['Official M HUB geometry; firmware 1.14 A0 slots equal these 80 keys + Fn. Fn is display-only.'],
                          omitted))
    # Royal Kludge RK68 HE (BY platform, IPI addressed protocol): driver pixel geometry.
    data, src = source('rk68he')
    for variant, layout, product in (('ANSI', 'ANSI', '110000000002'), ('ISO', 'UK', '11000000003C')):
        raw = data['layouts'][layout]
        pitch = 46 / 44.5  # driver key pitch ~44.5 px -> HallJoy 46 px
        keys = [key(rk_hid(k['value']), roundpx(k['x'] * pitch), roundpx(k['y'] * pitch),
                    roundpx((k['x'] + k['w']) * pitch) + 4, roundpx((k['y'] + k['h']) * pitch) + 4) for k in raw]
        common.require(len(keys) == (68 if layout == 'ANSI' else 69), 'RK68 HE key count')
        common.require(len({k['hid'] for k in keys}) == len(keys), 'RK68 HE duplicate factory keys')
        reports.append(report('Royal Kludge', 'RK68 HE', keys, src, 'ipi-addressed', [product],
                              ['Official RK web driver geometry (rk.hubx.pro). Physical IDs equal the firmware ID tables '
                               'of three published BY 68-key images; RK68 HE firmware itself is unpublished. Fn is display-only.'],
                              [], variant))
    # NuPhy Field75 HE: drive.nuphy.io unit grid, HallJoy 46 px pitch.
    data, src = source('nuphy')
    aliases = {'KC_FN1': (0x409, 'Fn'), 'KC_DOT': (55, '.')}
    keys, omitted = [], []
    for index, k in enumerate(data['keys']):
        name = k['keyCode']
        if index in data['excludedIndexes']:
            omitted.append(dict(code=name, reason='Excluded by the NuPhy driver from HE settings'))
            continue
        if name == 'KC_FN_WIN_AREA_SCREENSHOT':
            omitted.append(dict(code=name, reason='Firmware macro has no published UAP HID identity'))
            continue
        hid = aliases.get(name, common.CODES.get(name, (None, None)))[0]
        common.require(hid is not None, 'Unknown NuPhy action: ' + name)
        x, y = float(k['x']) * 46, float(k['y']) * 46
        keys.append(key(hid, roundpx(x), roundpx(y), roundpx(x + float(k.get('w', 1)) * 46),
                        roundpx(y + float(k.get('h', 1)) * 46)))
    common.require(len(keys) == 82, 'Field75 key count')
    reports.append(report('NuPhy', 'Field75 HE', keys, src, 'manual-layout', [],
                          ['Official NuPhy driver geometry. G1..G8 are excluded by the driver; manual selection only, as for Air60/Air75 HE.'],
                          omitted))
    # SteelSeries Apex Pro (1038:1610): GG CSS zones, 26x28 px keys on a 34 px pitch.
    data, src = source('gg')
    s = 46 / 34
    keys, omitted = [], []
    for code, rule in data['rules'].items():
        hid = int(code)
        if rule.get('display') == 'none':
            continue
        if hid == 101:
            omitted.append(dict(code='Menu', reason='GG places this zone over the arrow cluster; not a separate US key'))
            continue
        if hid == 0xF0:
            hid = 0x409  # SteelSeries OEM key; the backend does not own F0.
        x, y = float(rule['left'][:-2]), float(rule['top'][:-2])
        w = float(rule.get('width', '26px')[:-2])
        h = float(rule.get('height', '28px')[:-2])
        keys.append(key(hid, roundpx(x * s), roundpx(y * s), roundpx((x + w + 8) * s), roundpx((y + h + 6) * s)))
    common.require(len(keys) == 104, 'Apex Pro key count')
    reports.append(report('SteelSeries', 'Apex Pro', keys, src, 'steelseries-apex', ['APEXPRO-1038-1610'],
                          ['Official SteelSeries GG geometry for the original full-size Apex Pro, US region.'],
                          omitted))
    return reports


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--extract', action='store_true')
    parser.add_argument('--write-new', action='store_true')
    args = parser.parse_args()
    if args.extract:
        extract()
    for r in prepare():
        path = REPORTS / (r['id'] + '.json')
        data = (json.dumps(r, indent=2) + '\n').encode()
        if args.write_new and not path.exists():
            REPORTS.mkdir(parents=True, exist_ok=True)
            with path.open('xb') as f:
                f.write(data)
        else:
            common.require(path.read_bytes() == data, 'Report differs: ' + path.name)
        print(r['name'], len(r['keys']), 'PASS')


if __name__ == '__main__':
    sys.exit(main())
