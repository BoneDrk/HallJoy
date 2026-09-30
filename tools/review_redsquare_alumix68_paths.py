"""Offline candidate sweep; exact RSQ-20058 v1.30 only. No HID I/O."""
import argparse, hashlib, json, struct
from pathlib import Path
from review_redsquare_alumix68_firmware import Machine, HASH
from firmware_replay import ReplayError
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_PC

def request(m,c,length=0,offset=0):
 packet=bytearray(64);packet[:5]=bytes([170,c,length,offset&255,offset>>8]);packet[6]=1
 m.u.mem_write(0x20006258,bytes(packet));m.byte(0x20000479,1)
 m.run(0xe40c)
 return bytes(m.u.mem_read(0x20006218,64))

def main():
 ap=argparse.ArgumentParser();ap.add_argument('image',type=Path);ap.add_argument('output',type=Path);a=ap.parse_args()
 if not __debug__:raise RuntimeError('assertions required')
 b=a.image.read_bytes();assert hashlib.sha256(b).hexdigest()==HASH;assert not a.output.exists()
 rows=[]
 # Stop at side-effect subroutine boundaries instead of pretending flash/peripherals work.
 boundaries={0x4838:'flash_update',0x28a0:'calibration_finalize',0x292:'zero_fill',0x7100:'assignment_side_effect',0x45b8:'lighting_state_change'}
 for mode in (0,1):
  for length,offset in [(0,0),(8,0),(56,0x200)]:
   for c in range(256):
    m=Machine(b);m.byte(0x20000477,mode);hit=[]
    def hook(u,addr,size,_):
     if addr in boundaries:hit.append(boundaries[addr]);u.emu_stop()
    h=m.u.hook_add(UC_HOOK_CODE,hook)
    try:
     reply=request(m,c,length,offset);state='returned'
    except ReplayError as e:
     reply=b'';state=hit[0] if hit else 'unmodeled';fault=e.diagnostic if not hit else None
    finally:m.u.hook_del(h)
    row=dict(command=c,simulation=mode,length=length,offset=offset,result=state)
    if state=='unmodeled':row['fault']=fault
    if reply:row['reply_payload_nonzero']=any(reply[8:]);row['mode_after']=list(m.u.mem_read(0x20000476,2))
    rows.append(row)
 # Verify exact dynamic table read through unmodified full dispatcher.
 m=Machine(b);reads=0
 for frame in range(8):
  values=[(i*7+frame*43)%341 for i in range(128)]
  m.u.mem_write(0x20002c9c,struct.pack('<128H',*values))
  actual=bytearray()
  for off in range(0,256,56):
   n=min(56,256-off);r=request(m,0x16,n,0x200+off)
   assert r[:5]==bytes([85,0x16,n,(0x200+off)&255,(0x200+off)>>8])
   actual.extend(r[8:8+n]);reads+=1
  assert bytes(actual)==struct.pack('<128H',*values)
  assert bytes(m.u.mem_read(0x20000476,2))==bytes(2)
 # Read immutable scan map through the same read command family, then deliver response via real scheduler.
 scan=b[0x159d2:0x15a52];assert len(set(scan)-{125})==68 and sum(x!=125 for x in scan)==68
 copied=bytearray()
 for off in range(0,128,56):
  n=min(56,128-off);reply=request(m,0x12,n,0xc3d2+off);copied.extend(reply[8:8+n])
 assert copied==scan
 # Two distinct depths and a release arrive in one response with modes disabled.
 m.half(0x20002c9c+2,93);m.half(0x20002c9c+6,170);m.half(0x20002c9c+8,0)
 reply=request(m,0x16,56,0x200)
 assert struct.unpack_from('<H',reply,10)[0]==93 and struct.unpack_from('<H',reply,14)[0]==170 and struct.unpack_from('<H',reply,16)[0]==0
 m.sent.clear();m.run(0x133c0);assert m.sent==[reply]
 # The only nonscratch RAM change by a clean read is the reply-pending flag.
 before=bytes(m.u.mem_read(0x20000000,0x1e000));request(m,0x16,56,0x200);after=bytes(m.u.mem_read(0x20000000,0x1e000))
 changed=[0x20000000+i for i,(x,y) in enumerate(zip(before,after)) if x!=y]
 assert all(x==0x20000478 or 0x20006218<=x<0x20006298 or 0x2001d000<=x<0x2001e000 for x in changed)
 result=dict(image_sha256=HASH,status='PASS',opcode_scenarios=len(rows),read_transactions=reads,
  full_table_frames=8,scan_map_unique_keys=68,usb_response_verified=True,clean_read_no_unrelated_ram_changes=True,dynamic_values_checked=1024,
  candidate='0x16 + offset0x200 reads current depth RAM128 LE16 values without enabling modes',
  scope='Exact68 dispatcher only, synthetic RAM input. Does not prove104 offsets, coherent atomic full-frame sampling, physical timing, or USB delivery.',
  paths=rows,source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
 a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:v for k,v in result.items() if k!='paths'},indent=2))
if __name__=='__main__':main()
