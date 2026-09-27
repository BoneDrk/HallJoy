"""Record exact ABI1 DLL/link-map hashes; reject unreviewed Soup or ABI0 code."""
import argparse
import hashlib
import json
from pathlib import Path
import re

REVIEWED_SOUP = {'AnalogueKeyboard.o', 'DigitalKeyboard.o', 'Process.o',
                 'Thread.o', 'alloc.o', 'base.o', 'hwHid.o'}


def inspect_map(text):
    members = set(re.findall(r'\bsoup:([^\s]+)', text))
    if members != REVIEWED_SOUP:
        raise ValueError('Soup linked member set changed; review licenses: ' + repr(sorted(members)))
    if re.search(r'wooting_analog_common|compiler_builtins|rust_eh|TinyPngOut', text, re.I):
        raise ValueError('Unreviewed/legacy code linked into ABI1')
    if not re.search(r'\bmain\.o\b', text):
        raise ValueError('Plugin object absent from link map')
    return sorted(members)


def verify_record(record, dll, link_map):
    members = inspect_map(link_map.decode('utf8'))
    if (record.get('schema') != 1 or record.get('abi') != 1
            or record.get('dll_sha256') != hashlib.sha256(dll).hexdigest()
            or record.get('link_map_sha256') != hashlib.sha256(link_map).hexdigest()
            or record.get('soup_objects') != members
            or record.get('legacy_common_linked') is not False):
        raise ValueError('UAP DLL/map provenance mismatch; rebuild plugin from reviewed sources')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll', type=Path, required=True)
    parser.add_argument('--map', type=Path, required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--output', type=Path)
    mode.add_argument('--record', type=Path)
    args = parser.parse_args()
    dll, link_map = args.dll.read_bytes(), args.map.read_bytes()
    if not dll.startswith(b'MZ'):
        raise ValueError('Not a Windows DLL')
    members = inspect_map(link_map.decode('utf8'))
    record = {'schema': 1, 'abi': 1, 'dll_sha256': hashlib.sha256(dll).hexdigest(),
              'link_map_sha256': hashlib.sha256(link_map).hexdigest(), 'soup_objects': members,
              'legacy_common_linked': False,
              'scope': 'Exact build output; not a clearance claim for old shipped binaries or all rights.'}
    if args.record:
        verify_record(json.loads(args.record.read_text(encoding='utf8')), dll, link_map)
    else:
        with args.output.open('x', encoding='utf8') as f:
            json.dump(record, f, indent=2)
    print('UAP_LINK_CLOSURE=PASS abi=1 soup_objects=7 legacy_common=absent sha256=' + record['dll_sha256'])


if __name__ == '__main__':
    main()
