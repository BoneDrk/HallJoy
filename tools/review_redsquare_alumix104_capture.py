"""Replay exact partial Alumix104 capture; never execute invented missing code."""
import argparse, hashlib, json, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.local/ipi-reverse-deps'))
from firmware_replay import thumb_machine,run_until,ReplayError
from unicorn import UC_PROT_READ,UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE
HASH='0e3daac352b39edfd2def4dc54ed4eba74f016121df63ec854ae2e8e5e8d3c5e'
ENTRY,RETURN=0xe388,0xe92e
RX,TX,FLAGS=0x200065b4,0x20006574,0x200002fc
BOUNDARIES={0x391c,0x3bf8,0x6120,0x260,0x292,0x1e44,0x7c30,0xa90,0x934,0xf604,0xf4e4,0x7d64,0x7cf0}
BASES={0x10:0x9000,0x11:0x9200,0x12:0x9600,0x14:0x9a00,0x15:0x9c00,0x16:0xb000,0x17:0xb600,0x18:0xb200,0x1c:0xbc00}
def case(data,command,offset=0,length=8,rgb=0):
 u=thumb_machine(data,0xc000)
 # Missing config data is synthetic, marked separately; it is never executable.
 u.mem_map(0x9000,0x3000,UC_PROT_READ)
 synthetic=bytes((i*17+3)%256 for i in range(0x3000))
 u.mem_write(0x9000,synthetic)
 packet=bytearray(64);packet[:5]=bytes([0xaa,command,length,offset&255,offset>>8]);packet[6]=1
 u.mem_write(RX,bytes(packet));u.mem_write(FLAGS+4,b'\x01');u.mem_write(0x200000ac,bytes([rgb]))
 reads=[];writes=[]
 def read(m,access,a,n,v,other):
  if 0x9000<=a<0x19000:reads.append(a)
 def write(m,access,a,n,v,other):writes.append((a,n))
 u.hook_add(UC_HOOK_MEM_READ,read);u.hook_add(UC_HOOK_MEM_WRITE,write)
 try:stop=run_until(u,ENTRY,BOUNDARIES|{RETURN}|({0xe4f0} if command in (0x10,0x11) else set()));fault=None
 except ReplayError as e:stop=None;fault=e.diagnostic
 return dict(command=command,offset=offset,length=length,rgb=rgb,stop=hex(stop) if stop else None,
             reads=reads,writes=writes,reply=bytes(u.mem_read(TX,64)).hex(),fault=fault)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('capture',type=Path);ap.add_argument('output',type=Path);a=ap.parse_args()
 data=a.capture.read_bytes();assert hashlib.sha256(data).hexdigest()==HASH
 assert not a.output.exists()
 checks=[]
 for cmd,base in BASES.items():
  for offset in (0,0x200,0x3000):
   r=case(data,cmd,offset,8);reply=bytes.fromhex(r['reply'])
   assert r['stop']==hex(0xe4f0 if cmd in (0x10,0x11) else RETURN),(cmd,r)
   source=base+offset
   expected=data[source-0xc000:source-0xc000+8] if source>=0xc000 else bytes(((source-0x9000+i)*17+3)%256 for i in range(8))
   assert reply[8:16]==expected,(cmd,offset)
   assert list(range(source,source+8))==[v for v in r['reads'] if source<=v<source+8][:8]
   assert all((TX<=addr<TX+64) or addr in (FLAGS+3,FLAGS+4) or 0x2001e000<=addr<0x2001f000 for addr,n in r['writes']),r
   checks.append(r)
 # Actual captured bytes replayed through the unmodified command12 read path.
 for offset in range(0x2a00,0xfa00,56):
  n=min(56,0xfa00-offset);r=case(data,0x12,offset,n)
  assert r['stop']==hex(RETURN) and bytes.fromhex(r['reply'])[8:8+n]==data[offset-0x2a00:offset-0x2a00+n]
 sweep=[case(data,c,rgb=rgb) for rgb in (0,1) for c in range(256)]
 result=dict(status='PASS',capture_sha256=HASH,entry=hex(ENTRY),flash_bases={hex(k):hex(v) for k,v in BASES.items()},
  read_cases=len(checks),exact_code_blocks=951,dispatch_cases=len(sweep),read_checks=checks,dispatch=sweep,
  findings={'command16':'flash B000+offset, NOT live RAM; neighboring68 depth method does not apply',
   'low_code':'all established plain read bases >=9000 with unsigned16 offset; cannot reach0000..8FFF',
   'rgb':'active live-RGB state causes common cleanup call391C even before read; unexecuted missing code',
   'simulation':'66/67 toggle RAM flag; producer/typing path lies outside captured code, suitability unknown'},
  scope='Partial real code C000..18FFF; synthetic configuration9000..BFFF and zero RAM; all missing/external calls STOP, never fabricated. No ADC/USB/typing timing proof.',
  source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
 a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:v for k,v in result.items() if k not in ('read_checks','dispatch')},indent=2))
if __name__=='__main__':main()
