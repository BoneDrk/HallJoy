"""Execute the official Apex Pro 4.16.8 read handlers, offline, with synthetic ADC RAM.
No USB emulation, vendor software execution, device access or firmware redistribution.
Usage: python tools/verify_apex_pro_firmware.py path/to/keyboard-app.bin
"""
import sys, hashlib, struct
from pathlib import Path
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_MEM_WRITE
from unicorn.arm_const import *
b = Path(sys.argv[1]).read_bytes()
profiles={
 "750e0b987eac9c6f9596db882dc4ce423e63994b8082b957571ddaeb69ea5bbc": (0x1610,0x20001348,0x23dd4,0x0800b7c8,0x0800b42c,0x0801199c,0x22314,68),
 "5fbd2253f0217692d0ad23c95c852dec0ce2df817cc37c39b10e1d5717e8d796": (0x1614,0x20001348,0x23da8,0x0800b744,0x0800b3a8,0x080119a0,0x222e8,68),
 "4df0a1da0d9ca285cf1eb9f2a5f803b0c52a55a68abcddad7777159bc838280c": (0x1640,0x2000134c,0x24774,0x0801227c,0x08011ee0,0x0800c004,0x2105c,69),
}
pid,STATE,map_addr,depth_addr,range_addr,version_addr,table_addr,map_count=profiles[hashlib.sha256(b).hexdigest()]
BASE=0x0800a000; REQ=0x20010000; OUT=REQ+0x100; LEN=REQ+0x200
mapping={h:v-48 for h,v in enumerate(b[map_addr-0xa000:map_addr-0xa000+256]) if 48<=v<118}
assert mapping[0x1a]==16 and mapping[0x16]==30 and len(mapping)==map_count
assert len(set(mapping.values()))==68
# Exact FFC0:1 descriptor:64 input/output,642 feature bytes, plus Windows report ID.
assert bytes.fromhex('06c0ff0901a10106c1ff150026ff00750809f09540810209f19540910209f2968202b102c0') in b
assert b'\x38\x10'+struct.pack('<H',pid) in b
assert bytes.fromhex('09040100010300000009211101000122250007058203400001') in b
expected=[53,30,31,32,33,34,35,36,37,38,39,45,46,137,43,20,26,8,21,23,28,24,12,18,19,47,48,49,57,4,22,7,9,10,11,13,14,15,51,52,50,40,225,100,29,27,6,25,5,17,16,54,55,56,135,229,224,227,226,139,44,138,136,230,231,240,228,42]
assert all(mapping[h]==i for i,h in enumerate(expected))

def invoke(addr, req):
 u=Uc(UC_ARCH_ARM,UC_MODE_THUMB);u.mem_map(0x08000000,0x40000);u.mem_write(BASE,b)
 u.mem_map(0x20000000,0x20000)
 for i in range(70):
  for off,value in [(2*i,1100+i),(0x24c+32*i,1200+i),(0x8c+2*i,3000+i),(0x118+2*i,1000+i)]:
   u.mem_write(STATE+off,struct.pack('<H',value))
 u.mem_write(REQ,req+bytes(64-len(req)));u.mem_write(LEN,struct.pack('<I',64))
 before=bytes(u.mem_read(STATE,0xb00));writes=[]
 def watch(uc,access,address,size,value,data):
  assert OUT<=address<OUT+64 or LEN<=address<LEN+4 or 0x20017000<=address<0x20018000, hex(address)
 u.hook_add(UC_HOOK_MEM_WRITE,watch)
 for reg,value in [(UC_ARM_REG_R0,REQ),(UC_ARM_REG_R1,64),(UC_ARM_REG_R2,OUT),(UC_ARM_REG_R3,LEN),(UC_ARM_REG_SP,0x20018000),(UC_ARM_REG_LR,0x08030001)]:u.reg_write(reg,value)
 u.emu_start(addr|1,0x08030000,count=100000)
 assert u.reg_read(UC_ARM_REG_PC)==0x08030000
 assert bytes(u.mem_read(STATE,0xb00))==before
 return struct.unpack('<I',u.mem_read(LEN,4))[0],bytes(u.mem_read(OUT,64))
for bank in range(1,6):
 n,p=invoke(depth_addr,bytes([0xd7,bank]));assert n==56
 assert struct.unpack('<28H',p[:56])==tuple(range(1100+(bank-1)*14,1114+(bank-1)*14))+tuple(range(1200+(bank-1)*14,1214+(bank-1)*14))
for bank in [0,6,255]:assert invoke(depth_addr,bytes([0xd7,bank]))[0]==0
items=list(mapping.items())
for start in range(0,len(items),12):
 keys=items[start:start+12];req=bytes([0xda,len(keys)])+b''.join(bytes([h,0,0,0,0]) for h,i in keys)
 n,p=invoke(range_addr,req);assert n==len(keys)*5
 assert p[:n]==b''.join(struct.pack('<BHH',h,3000+i,1000+i) for h,i in keys)
assert invoke(range_addr,bytes([0xda,13]))[0]==0
n,p=invoke(version_addr,bytes([0x90,0]));assert p.startswith(b'4.16.8\0')
# Verify exact command dispatch table (bit7 selector reviewed in disassembly).
for cmd,addr in [(0x57,depth_addr|1),(0x5a,range_addr|1),(0x10,version_addr|1)]:
 records=[struct.unpack_from('<III',b,table_addr-0xa000+j*12) for j in range(64)]
 assert next(r[1] for r in records if r[0]==cmd)==addr
print('PASS: official ARM handlers: 5 ADC banks, all ROM HID calibration mappings, invalid requests, version; sensor RAM unchanged')
print('Sensor-to-HID:',[next((h for h,j in items if i==j),0) for i in range(70)])

if pid!=0x1610: sys.exit(0)

# Reproduce the cross-protocol reset entirely offline. Only the delay routine is
# bypassed; the official command handler and reset-register write execute intact.
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB);u.mem_map(0x08000000,0x40000);u.mem_write(BASE,b)
u.mem_map(0x20000000,0x20000);u.mem_map(0x40000000,0x30000);u.mem_map(0xe000e000,0x2000)
u.mem_write(REQ,bytes([1,2])+bytes(62));u.reg_write(UC_ARM_REG_R0,REQ);u.reg_write(UC_ARM_REG_SP,0x20018000)
reset=[]
from unicorn import UC_HOOK_CODE
def delay(uc,address,size,data):
 if address==0x0801143c:uc.reg_write(UC_ARM_REG_PC,uc.reg_read(UC_ARM_REG_LR))
def reset_write(uc,access,address,size,value,data):
 if address==0xe000ed0c:reset.append(value);uc.emu_stop()
u.hook_add(UC_HOOK_CODE,delay);u.hook_add(UC_HOOK_MEM_WRITE,reset_write)
reset_entry=next(r for r in records if r[0]==1)
assert reset_entry[1:]==(0x08011219,0x08011219)
u.emu_start(reset_entry[2],0x08030000,count=100000)
assert reset==[0x05fa0004],reset
print('PASS: foreign SparkLink packet 01 02 executes Apex Pro SYSRESETREQ (offline ARM execution)')
