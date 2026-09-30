"""Scoped execution of official RSQ-20058 v1.30; NEVER proof of Alumix104 firmware."""
import argparse, hashlib, json, struct, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.local/ipi-reverse-deps'))
from firmware_replay import thumb_machine, run_until
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import *
HASH='020778ba842603c49fe21365ad10380dd877f9be29ecaeefb9760eeab69461c1'
class Machine:
 def __init__(self,data):
  self.u=thumb_machine(data,0);self.sent=[];self.busy=0
  self.u.hook_add(UC_HOOK_CODE,self.hook)
 def hook(self,u,a,n,_):
  if a==0x10e54:
   assert u.reg_read(UC_ARM_REG_R0)==2 and u.reg_read(UC_ARM_REG_R2)==64
   self.sent.append(bytes(u.mem_read(u.reg_read(UC_ARM_REG_R1),64)))
   u.reg_write(UC_ARM_REG_R0,self.busy);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
 def byte(self,a,v):self.u.mem_write(a,bytes([v]))
 def half(self,a,v):self.u.mem_write(a,struct.pack('<H',v))
 def run(self,a,stops=(0x200,)):
  self.u.reg_write(UC_ARM_REG_SP,0x2001e000);self.u.reg_write(UC_ARM_REG_LR,0x201)
  return run_until(self.u,a,set(stops))
 def command(self,c):
  self.u.mem_write(0x20006258,bytes([0xaa,c])+bytes(62));self.byte(0x20000479,1)
  self.run(0xe40c)
  return list(self.u.mem_read(0x20000476,2))
 def tail(self,index,selected,travel,selected_travel):
  for reg,val in [(UC_ARM_REG_R0,index),(UC_ARM_REG_R1,travel),(UC_ARM_REG_R4,index),(UC_ARM_REG_R5,0x20003300),(UC_ARM_REG_R6,0x20000476),(UC_ARM_REG_R7,0x20000110),(UC_ARM_REG_R9,0x2000309c),(UC_ARM_REG_R10,0x20000114)]:self.u.reg_write(reg,val)
  self.byte(0x20000110,index);self.byte(0x20000117,selected);self.byte(0x20000119,0)
  self.half(0x20002f9c+2*selected,selected_travel)
  self.half(0x20002c9c+2*index,travel)
  self.half(0x20003300+2*index,1800);self.half(0x2000309c+2*index,2200);self.half(0x2000371c+2*index,2100)
  self.run(0x4e1c,(0x4f26,))
  return bool(self.u.mem_read(0x20000119,1)[0]),bytes(self.u.mem_read(0x200042e4,64))
def main():
 ap=argparse.ArgumentParser();ap.add_argument('image',type=Path);ap.add_argument('output',type=Path);a=ap.parse_args()
 if not __debug__:raise RuntimeError('assertions required')
 data=a.image.read_bytes();assert hashlib.sha256(data).hexdigest()==HASH
 assert not a.output.exists()
 m=Machine(data);rows=[]
 for c,expected in [(0x66,[0,1]),(0x67,[0,0]),(0x66,[0,1])]:
  actual=m.command(c);assert actual==expected;rows.append(dict(command=hex(c),calibration=actual[0],simulation=actual[1]))
 checks=0
 for index in range(68):
  for depth in (0,1,9,10,11,93,170,340):
   ready,p=m.tail(index,index,depth,50)
   assert ready and p[:3]==bytes([85,251,index]) and int.from_bytes(p[10:12],'little')==depth
   checks+=1
 ready,p=m.tail(1,0,93,170);assert not ready
 # The ordinary branch gate tests calibration, not the simulation flag.
 m.byte(0x200003e1,1);m.byte(0x200000a9,1)
 reached=m.run(0x4f26,(0x4f40,0x4b30));assert reached==0x4f40
 # Producer marks pending; actual USB routine retains it on modeled busy, clears on success.
 ready,p=m.tail(0,0,170,170);assert ready
 m.byte(0x20000478,0);m.busy=2;m.run(0x133c0);assert m.u.mem_read(0x20000119,1)[0]==1
 m.busy=0;m.run(0x133c0);assert m.u.mem_read(0x20000119,1)[0]==0
 assert m.command(0x67)==[0,0]
 ready,p=m.tail(0,0,170,170);assert not ready
 result=dict(status='PASS',image_sha256=HASH,commands=rows,tail_cases=checks,nonselected_key_emits=False,normal_processing_gate_with_simulation='reached',usb_busy_retains_pending=True,stop_disables_production=True,
 scope='Exact68 firmware dispatcher, report tail and USB scheduler; synthetic already-computed travel and selected-key state, modeled USB return. No full ADC/scan/typing-HID/timing emulation. Not104 firmware.',
 restriction='Simulation tail reports selected key only; static4c80..4c98 selects greatest processed depth. Calibration branch additionally reports nonselected keys but must not be used for gameplay.',
 source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest())
 a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
