"""Validate private Red Square code-window export; never treats similarity as support."""
import argparse, hashlib, json, re
from pathlib import Path
FIRST, END = 0x2a00, 0xfa00

def decode(text):
 lines=text.splitlines()
 records=[line.split(' redsquare.research ',1)[1] for line in lines if ' redsquare.research ' in line]
 if records:
  starts=[i for i,line in enumerate(records) if line.startswith('HallJoy RedSquare code probe v1;')]
  if not starts:raise ValueError('capture start missing')
  lines=records[starts[-1]:]
 if not lines or not lines[0].startswith('HallJoy RedSquare code probe v1;'):raise ValueError('unknown capture schema')
 if not any(s=='matched=1' for s in lines):raise ValueError('no unique exact target')
 if not any(s.startswith('model=Alumix ') and 'vid=0c45 pid=80ac' in s for s in lines):raise ValueError('target identity missing')
 if not lines[-1].startswith('complete blocks='):raise ValueError('incomplete acquisition')
 out=bytearray();count=0
 for line in lines:
  if not line.startswith('block '):continue
  m=re.fullmatch(r'block ([0-9a-f]{4}) ([0-9a-f]+)',line)
  if not m:raise ValueError('invalid block')
  off=int(m[1],16);b=bytes.fromhex(m[2])
  if off!=FIRST+len(out) or not 1<=len(b)<=56 or off+len(b)>END:raise ValueError('gap, overlap or invalid length')
  out.extend(b);count+=1
 if len(out)!=END-FIRST or lines[-1]!=f'complete blocks={count}':raise ValueError('size/count mismatch')
 return bytes(out)

def main():
 ap=argparse.ArgumentParser();ap.add_argument('capture',type=Path);ap.add_argument('--reference',type=Path);a=ap.parse_args()
 b=decode(a.capture.read_text(encoding='utf-8'));r=dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest(),complete=True,
 interpretation='Command-relative offsets only. Flash-base9600 is established on68, not automatically104. No support promotion.')
 if a.reference:
  ref=a.reference.read_bytes()[0xc000:0x19000]
  if len(ref)!=len(b):raise ValueError('reference window missing')
  r['reference_equal_bytes']=sum(x==y for x,y in zip(b,ref));r['reference_exact_match']=b==ref
 print(json.dumps(r,indent=2))
if __name__=='__main__':main()
