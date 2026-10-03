"""A keyboard may be Supported only when HallJoy ships a layout for it.

docs/development/supported_layouts.json lists every Supported Sheet model and the
built-in presets that draw it.

- `catalog`: every listed model has at least one preset, every preset exists
  in the production-compiled catalog (layout-catalog.tsv from the profile
  simulator, see tools/run_profile_transaction_tests.ps1), and the model can be
  found by name in the Brand/Model picker (its Sheet name, or a reviewed
  `shownAs` name).
- `--sheet`: the Supported rows of a fresh Sheet read are exactly the listed
  models. Accepts a native get_spreadsheet snapshot or derived
  [{brand, model, status}] rows.

Order of work for a new Supported keyboard: add its layout, add it here, run both
checks, and only then set the Sheet status.
"""
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIST = ROOT / 'docs/development/supported_layouts.json'


def listed():
    data = json.loads(LIST.read_text(encoding='utf-8'))
    if data.get('schema') != 1:
        raise SystemExit('Unknown supported_layouts.json schema')
    return data['models']


def sheet_rows(path):
    data = json.loads(Path(path).read_text(encoding='utf-8'))
    if isinstance(data, list):
        return [(r['brand'], r['model'], r['status']) for r in data]
    sheet = next(s for s in data.get('updatedSpreadsheet', data)['sheets'] if s['properties']['title'] == 'Main')
    rows = []
    for row in sheet['data'][0].get('rowData', [])[1:]:
        values = [c.get('formattedValue', '') for c in row.get('values', [])[:3]]
        if len(values) == 3 and all(values):
            rows.append(tuple(values))
    return rows


def normalized(text):
    return re.sub('[^a-z0-9]', '', text.lower())


def check_catalog(models, tsv):
    # Columns: brand, preset name, name shown in the Brand/Model picker, ...
    shown = {}
    for line in Path(tsv).read_text(encoding='utf-8').splitlines():
        if line:
            columns = line.split('\t')
            shown[columns[1]] = columns[2]
    failures = []
    for m in models:
        if not m['presets']:
            failures.append(f"{m['brand']} {m['model']}: no layout preset")
        for preset in m['presets']:
            if preset not in shown:
                failures.append(f"{m['brand']} {m['model']}: preset not built in: {preset}")
        # The user must be able to find the keyboard by name in the picker.
        wanted = normalized(m.get('shownAs', m['model']))
        if not any(wanted in normalized(shown.get(p, '')) for p in m['presets']):
            failures.append(f"{m['brand']} {m['model']}: name not shown in the layout picker "
                            f"{sorted({shown.get(p, '?') for p in m['presets']})}")
    return failures


def check_sheet(models, path):
    supported = {(b, m) for b, m, status in sheet_rows(path) if status.startswith('Supported')}
    known = {(m['brand'], m['model']) for m in models}
    failures = [f'{b} {m}: Supported in the Sheet but has no layout entry' for b, m in sorted(supported - known)]
    failures += [f'{b} {m}: listed but not Supported in the Sheet' for b, m in sorted(known - supported)]
    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('catalog', nargs='?', help='layout-catalog.tsv exported by the profile simulator')
    parser.add_argument('--sheet', help='fresh Sheet snapshot (native or derived rows)')
    args = parser.parse_args()
    if not args.catalog and not args.sheet:
        parser.error('give a catalog TSV, --sheet, or both')
    models = listed()
    failures = []
    if args.catalog:
        failures += check_catalog(models, args.catalog)
    if args.sheet:
        failures += check_sheet(models, args.sheet)
    for failure in failures:
        print(' - ' + failure)
    print(f"SUPPORTED_LAYOUTS={'FAIL' if failures else 'PASS'} models={len(models)}"
          f" catalog={bool(args.catalog)} sheet={bool(args.sheet)}")
    return 1 if failures else 0


if __name__ == '__main__':
    raise SystemExit(main())
