"""Inventory distribution provenance. Classification is triage, never legal clearance."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import zipfile


def category(path):
    p=Path(path);ext=p.suffix.lower()
    if path.startswith('src/HallJoyProject/third_party/'):
        return 'third-party-runtime-conditions', 'Review exact library provenance, notices and source obligations'
    if path.startswith('third_party/UniversalAnalogPluginFixed/'):
        return 'third-party-runtime-conditions', 'MIT plugin/Soup; MPL common binaries require separate treatment'
    if path.startswith('firmware/'):
        return 'firmware-gpl-conditions', 'Separate QMK/Keychron GPL work; not solely HallJoy-owned'
    if path.startswith('docs/'):
        if ext in {'.bin','.hex','.dfu','.uf2','.elf','.asm','.gz'}:
            return 'redistribution-not-established', 'Vendor executable/extracted content; source license not established by download'
        if ext in {'.js','.css','.html','.pdf','.png','.jpg','.jpeg','.svg'}:
            return 'redistribution-not-established', 'Copied web software/artwork/manual: verify source-specific permission'
        if ext=='.zip':
            return 'archive-review', 'Inspect members; root license does not license third-party contents'
        if path.startswith('docs/research/'):
            return 'research-provenance-review', 'May contain upstream code/catalogs or project analysis; per-source review required'
    if ext in {'.ico','.png','.jpg','.svg','.ttf','.otf'}:
        return 'artwork-provenance-pending', 'Owner provenance or applicable asset license required'
    if ext in {'.lib','.a','.dll','.exe','.bin','.zip'}:
        return 'unclassified-binary-review', 'Do not infer license from extension or repository license'
    return 'project-license-declared', 'AGPL declaration is not proof of exclusive authorship; specific notices override'


def inventory(root, tree):
    if tree.get('truncated') is not False: raise ValueError('complete GitHub tree required')
    rows=[]
    for item in tree['tree']:
        if item['type']!='blob': continue
        name=item['path'];p=(root/name).resolve()
        if not p.is_relative_to(root.resolve()): raise ValueError('path escape')
        status,reason=category(name)
        row=dict(path=name,github_blob=item['sha'],published_bytes=item.get('size'),
                 category=status,reason=reason,local_present=p.is_file())
        if p.is_file():
            data=p.read_bytes()
            row['local_sha256']=hashlib.sha256(data).hexdigest()
            def blob(b):return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
            row['local_matches_published']=blob(data)==item['sha']
            row['matches_after_crlf_normalization']=blob(data.replace(b'\r\n',b'\n'))==item['sha']
            if p.suffix=='.zip':
                with zipfile.ZipFile(p) as z:
                    row['archive_members']=[dict(path=x.filename,size=x.file_size) for x in z.infolist() if not x.is_dir()]
        rows.append(row)
    return dict(schema=1,published_tree=tree['sha'],scope='All published main-tree file paths; local content compared, not all historic commits/assets.',
                legal_clearance=False,files=len(rows),counts=dict(Counter(x['category'] for x in rows)),inventory=rows)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--tree',type=Path,required=True)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();r=inventory(a.root,json.loads(a.tree.read_text(encoding='utf8')))
    with a.output.open('x',encoding='utf8') as f:json.dump(r,f,indent=2)
    print(json.dumps({k:v for k,v in r.items() if k!='inventory'},indent=2))


if __name__=='__main__':main()
