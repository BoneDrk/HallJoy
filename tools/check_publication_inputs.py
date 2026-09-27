"""Reject unreviewed research payloads in a PUBLICATION checkout's Git index."""
import argparse
import json
from pathlib import Path, PurePosixPath
import subprocess


def violations(names, denied):
    findings = []
    payloads = {'.bin', '.hex', '.dfu', '.uf2', '.elf', '.asm', '.gz',
                '.js', '.css', '.html', '.pdf', '.png', '.jpg', '.jpeg', '.svg'}
    for name in names:
        path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or '\\' in name:
            findings.append((name, 'unsafe path'))
        elif name in denied:
            findings.append((name, 'redistribution permission not established'))
        elif name.startswith(('.local/', '.cache/', '.analysis/', 'outputs/')):
            findings.append((name, 'private/generated content'))
        elif name.startswith('docs/') and path.suffix.lower() in payloads:
            findings.append((name, 'new vendor/research payload requires review'))
    return findings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()
    # An extracted workspace has no authoritative publication index: fail closed.
    actual = subprocess.check_output(['git', '-C', str(root), 'rev-parse', '--show-toplevel']).decode().strip()
    if Path(actual).resolve() != root:
        raise SystemExit('Run against the publication repository itself, not its parent.')
    policy = json.loads((root / 'docs/legal/PUBLICATION_POLICY.json').read_text(encoding='utf8'))
    names = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z']).decode('utf8').split('\0')
    failed = violations([x for x in names if x], set(policy['denied_paths']))
    for name, reason in failed:
        print(reason + ': ' + name)
    print('PUBLICATION_INPUTS=' + ('FAIL' if failed else 'PASS') + '; not a legal clearance certificate')
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
