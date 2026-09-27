"""Compact verified evidence view; raw evidence stays available via JSON pointers."""
import argparse
from collections import defaultdict
import json
from pathlib import Path
from firmware_behavior import validate, digest


def pointer(document, path):
    if path == '': return document
    if not path.startswith('/'): raise ValueError('JSON pointer must start with /')
    for part in path[1:].split('/'):
        part = part.replace('~1', '/').replace('~0', '~')
        document = document[int(part)] if isinstance(document, list) else document[part]
    return document


def summarize_samples(samples):
    """Group identical observed transfer curves; never infer untested resolution."""
    curves = defaultdict(list)
    for row in samples:
        curves[(row['setting'], row['key'])].append((row['input'], row['output']))
    groups = defaultdict(list)
    for (setting, key), curve in curves.items():
        groups[(setting, tuple(sorted(curve)))].append(key)
    return [dict(setting=setting, keys=sorted(keys), sampled_curve=curve,
                 smallest_sampled_nonzero_input=min((x for x,y in curve if y), default=None),
                 observed_distinct_outputs=len(set(y for x,y in curve)))
            for (setting, curve), keys in sorted(groups.items())]


def key_ranges(keys):
    ranges = []
    for key in sorted(set(keys)):
        if ranges and key == ranges[-1][1] + 1: ranges[-1][1] = key
        else: ranges.append([key, key])
    return ','.join(str(a) if a == b else f'{a}-{b}' for a,b in ranges)


def compact_groups(groups):
    return [dict(setting=g['setting'], keys=key_ranges(g['keys']),
                 sampled_points=len(g['sampled_curve']),
                 smallest_sampled_nonzero_input=g['smallest_sampled_nonzero_input'],
                 observed_distinct_outputs=g['observed_distinct_outputs'],
                 altered_samples=[(x,y) for x,y in g['sampled_curve'] if x != y]) for g in groups]


def waveform_summary(scenario):
    # Exact MAD68 synthetic capture contract: two consecutive same-key 6-bit
    # fragments, low then high. This is not a loss-tolerant USB decoder.
    malformed, decoded, last = 0, 0, None
    for row in scenario['passes']:
        reports = row['reports']
        if not reports: continue
        try:
            pair = [bytes.fromhex(x) for x in reports]
            if (len(pair) != 2 or any(len(x) != 3 or x[0] != 7 for x in pair)
                    or pair[0][1] != pair[1][1]):
                raise ValueError('outside paired capture contract')
            last = (pair[0][2] & 63) | ((pair[1][2] & 63) << 6)
            decoded += 1
        except ValueError:
            malformed += 1
            last = None
    return dict(name=scenario['name'], passes=len(scenario['passes']),
                report_passes=sum(bool(x['reports']) for x in scenario['passes']),
                decoded_pairs=decoded, malformed_passes=malformed,
                final_reported_value=last)


def inspect(bundle):
    if (bundle / 'manifest.json').exists():
        manifest=json.loads((bundle/'manifest.json').read_text(encoding='utf8'))
        if manifest.get('schema')!=1 or manifest.get('kind')!='mad68-usb-lifecycle':
            raise ValueError('unrecognized evidence bundle kind')
        assets=manifest.get('assets',{})
        required={'raw.json','summary.json','mad68_usb_lifecycle_audit.py',
                  'mad68_v2_dual_firmware_audit.py','firmware_replay.py','firmware_usb_model.py'}
        if not required <= assets.keys(): raise ValueError('incomplete lifecycle evidence')
        for name,sha in assets.items():
            path=(bundle/name).resolve()
            if not path.is_relative_to(bundle.resolve()) or digest(path)!=sha:
                raise ValueError('stale or escaping evidence asset: '+name)
        raw=json.loads((bundle/'raw.json').read_text(encoding='utf8'))
        brief=json.loads((bundle/'summary.json').read_text(encoding='utf8'))
        alerts=[]
        for version in brief:
            for scenario in version['scenarios']:
                if scenario['blocked']:
                    alerts.append(dict(version=version['version'],scenario=scenario['name'],
                        finding='Ready wait exceeded instruction budget; resumed only after modeled readiness restored'))
                if len(scenario['release_accepted'])!=2:
                    alerts.append(dict(version=version['version'],scenario=scenario['name'],
                        finding='Incomplete accepted release pair; missing fragment not retried in40 observed per-key passes'))
        return dict(alerts=alerts, execution=manifest['execution'], scope=manifest['scope'],
                    limitations=manifest['limitations'], results=brief,
                    support_status='UNCHANGED; fault expectations are not suitability'),raw
    report = json.loads((bundle / 'assessment.json').read_text(encoding='utf8'))
    assessment = validate(report, bundle)
    raw = json.loads((bundle / report['assets']['raw']['path']).read_text(encoding='utf8'))
    result = dict(assessment=assessment, selection=report['selection'],
                  findings={k:v['detail'] for k,v in report['findings'].items()
                            if v['state'] in ('unknown', 'restricted')}, versions=[])
    known_curves = {}
    for index, version in enumerate(raw):
        groups = summarize_samples(version.get('simulation_samples', []))
        signature = json.dumps(groups, sort_keys=True)
        transfer = compact_groups(groups) if signature not in known_curves else {'same_as_version': known_curves[signature]}
        known_curves.setdefault(signature, version['version'])
        result['versions'].append(dict(version=version['version'],
            evidence_pointer='/' + str(index),
            normalization_samples=len(version.get('simulation_samples', [])),
            transfer_groups=transfer,
            stream_report_passes=[i for i,x in enumerate(version.get('stream_replay', [])) if x['reports']],
            scenarios=[dict(waveform_summary(s), pointer=f'/{index}/waveforms/{n}')
                       for n,s in enumerate(version.get('waveforms', []))]))
    return result, raw


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('bundle', type=Path)
    p.add_argument('--pointer', help='Retrieve only this raw-result JSON pointer')
    p.add_argument('--limit', type=int, default=12000, help='Maximum printed characters; larger output requires explicit selection')
    a=p.parse_args()
    summary, raw=inspect(a.bundle)
    value=pointer(raw,a.pointer) if a.pointer is not None else summary
    output=json.dumps(value,indent=2)
    if a.limit < 1: p.error('positive limit required')
    if len(output)>a.limit:
        print(json.dumps(dict(output_characters=len(output), limit=a.limit,
            action='Select a narrower --pointer or explicitly increase --limit; nothing silently truncated.')))
        return 2
    print(output)
    return 0


if __name__ == '__main__': raise SystemExit(main())
