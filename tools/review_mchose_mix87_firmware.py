"""Exact Mix87 III v1.22 branch replay; synthetic RAM and timer setup, not hardware."""
import argparse, hashlib, json, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_PC, UC_ARM_REG_LR, UC_ARM_REG_SP
from firmware_replay import thumb_machine, run_until
SHA = 'a49dbda3a4a041ab9a872d86eb2880a03d3f5c47bf6d95af616218fd07514688'
BASE = 0x08008000
BUF = 0x20007a20
CFG = 0x200079e0
MODE = 0x2000487c
FLASH = 0x0802c000

def machine(data):
    u = thumb_machine(data, BASE)
    # Synthetic valid stored/current configuration; no assertion about a user's settings.
    u.mem_write(FLASH, bytes(256))
    u.mem_write(CFG, bytes(64))
    for i in range(92):
        u.mem_write(0x200037ac+i*4, struct.pack('<I', 16384))
        u.mem_write(0x20003a8c+i*4, struct.pack('<I', 16200))
    u.effects = []
    def peripheral(m, address, size, _):
        if address == 0x0800841a:
            m.effects.append(dict(timer_setup_argument=m.reg_read(UC_ARM_REG_R0)))
            m.reg_write(UC_ARM_REG_PC, m.reg_read(UC_ARM_REG_LR))
    u.hook_add(UC_HOOK_CODE, peripheral)
    return u

def command(u, opcode, payload=b'', offset=0):
    report=bytearray(64);report[0]=0x55;report[1]=opcode;report[4]=len(payload)
    report[5:8]=offset.to_bytes(3,'little');report[8:8+len(payload)]=payload
    report[3]=(report[4]+sum(report[5:8+len(payload)]))&255
    u.mem_write(BUF, bytes(report));u.mem_write(0x20004891,b'\1')
    u.reg_write(UC_ARM_REG_SP,0x2001f000);u.reg_write(UC_ARM_REG_LR,BASE|1)
    result=run_until(u,0x080131f0,{BASE,0x08008754},budget=20000)
    return dict(opcode=hex(opcode),stop=result,flash_writer_reached=u.reg_read(UC_ARM_REG_PC)==0x08008754,
                mode=int.from_bytes(u.mem_read(MODE,1),'little'),debug=bool(u.mem_read(CFG+7,1)[0]&8),
                reply=bytes(u.mem_read(BUF,8)).hex(),baseline0=int.from_bytes(u.mem_read(0x20003a8c,4),'little'))

def main():
    parser=argparse.ArgumentParser();parser.add_argument('image',type=Path);parser.add_argument('output',type=Path);a=parser.parse_args()
    if not __debug__:raise RuntimeError('Assertions required')
    b=a.image.read_bytes();assert hashlib.sha256(b).hexdigest()==SHA
    if a.output.exists():raise RuntimeError('Use a new output path')
    u=machine(b);first=command(u,0xa8);assert first['mode']==1 and first['debug'] and not first['flash_writer_reached']
    second=command(u,0xa9);assert second['mode']==0 and second['debug'] and not second['flash_writer_reached']
    cleanup=command(u,6);assert cleanup['mode']==0 and not cleanup['debug'] and not cleanup['flash_writer_reached']
    u2=machine(b);normal_debug=command(u2,6,b'\x08',7);assert normal_debug['flash_writer_reached']
    # Validate the real one-byte update's prepared flash image for every profile.
    # Stop BEFORE erase/program: peripherals and power-loss safety are not modeled.
    flag_writes=[]
    from unicorn.arm_const import UC_ARM_REG_R1
    for profile in range(4):
        for enable in (False,True):
            t=machine(b);saved=bytearray((i*17+5)&255 for i in range(256))
            at=profile*64+7;saved[at]=(saved[at]&~8) if enable else (saved[at]|8)
            t.mem_write(FLASH,bytes(saved));t.mem_write(0x20003dbe,bytes([profile]))
            t.mem_write(CFG,bytes(saved[profile*64:(profile+1)*64]))
            value=(saved[at]|8) if enable else (saved[at]&~8)
            result=command(t,6,bytes([value]),at)
            assert result['flash_writer_reached']
            expected=saved[:];expected[at]=value
            assert bytes(t.mem_read(0x2000039c,256))==bytes(expected)
            assert t.reg_read(UC_ARM_REG_R0)==FLASH and t.reg_read(UC_ARM_REG_R1)==256
            flag_writes.append(dict(profile=profile,enable=enable,prepared_bytes=256,erase_sector_bytes=8192,other_config_bytes_preserved=True))
    # Isolate the production typing-diversion branch with controlled post-depth registers.
    typing=[]
    for mode in (0,1):
        t=machine(b);t.mem_write(MODE,bytes([mode]))
        from unicorn.arm_const import UC_ARM_REG_R9
        t.reg_write(UC_ARM_REG_R9,0)
        reached=run_until(t,0x0800cc68,{0x0800cc9e,0x0800d14c},budget=60)
        pc=t.reg_read(UC_ARM_REG_PC);typing.append(dict(mode=mode,normal_path=pc==0x0800cc9e,execution=reached))
        assert (pc==0x0800cc9e)==(mode==0)
    u3=machine(b);mapping=b[0x08015da6-BASE:0x08015da6-BASE+92*3];cases=0
    for i in range(92):
        if not mapping[i*3]:continue
        for sw in range(5):
            maximum=struct.unpack_from('<H',b,0x08013ba6-BASE+sw*12)[0]
            u3.mem_write(0x20009498+i*8,struct.pack('<I',sw))
            for depth in (0,1,9,10,17,100,maximum-6,maximum-5,maximum):
                u3.mem_write(0x20007520+i*4,struct.pack('<I',depth))
                u3.reg_write(UC_ARM_REG_R0,i);u3.reg_write(UC_ARM_REG_LR,BASE|1);u3.reg_write(UC_ARM_REG_SP,0x2001f000)
                run_until(u3,0x0800f406,{BASE},budget=300)
                report=bytes(u3.mem_read(0x200044a4,64));value=int.from_bytes(report[6:8],'big')
                expected=maximum if depth>=maximum-5 else 0 if depth<=9 else depth
                assert report[0]==0xa0 and report[1:4]==mapping[i*3:i*3+3] and value==expected
                assert int.from_bytes(report[14:16],'big')==maximum
                cases+=1
    # Execute every opcode through the real dispatcher, stopping before handlers.
    handlers={0x080135a2,0x08013568,0x08013582,0x08013336}
    for table,count in ((0x08013236,18),(0x080132ca,14),(0x08013300,5)):
        handlers.update(table+2*struct.unpack_from('<H',b,table-BASE+i*2)[0] for i in range(count))
    routes={}
    for frame in (0x55,0x5f):
        for op in range(256):
            t=machine(b);packet=bytearray(64);packet[0]=frame;packet[1]=op
            t.mem_write(BUF,bytes(packet));t.mem_write(0x20004891,b'\1')
            run_until(t,0x080131f0,handlers,budget=300)
            routes[f'{frame:02x}:{op:02x}']=hex(t.reg_read(UC_ARM_REG_PC))
    # Drive the actual service clock with synthetic ticks, keeping RAM between passes.
    stream=machine(b);stream.mem_write(CFG+7,b'\x08');service=[]
    for i,depth in ((89,20),(91,30),(88,40)):
        stream.mem_write(0x20007520+i*4,struct.pack('<I',depth))
    for step in range(1,15):
        stream.mem_write(0x200023fa,struct.pack('<H',step*10))
        stream.reg_write(UC_ARM_REG_LR,BASE|1)
        run_until(stream,0x0800e868,{BASE},budget=4000)
        pending=bool(stream.mem_read(0x2000487d,1)[0])
        if pending:
            service.append(dict(pass_number=step,report=bytes(stream.mem_read(0x200044a4,16)).hex()))
            stream.mem_write(0x2000487d,b'\0') # explicit modeled successful USB consumption
    assert [x['pass_number'] for x in service]==[11,12,13]
    # A read-only query resets the service quiet counter even with debug already on.
    command(stream,3)
    assert stream.mem_read(0x20007064,1)==b'\0'
    result=dict(image_sha256=SHA,source_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                flag_writes=flag_writes,opcode_routes=routes,service=service,command_resets_stream_counter=True,lifecycle=[first,second,cleanup],normal_debug=normal_debug,timer_model=u.effects,typing_branch=typing,
                report_cases=cases,active_slots=sum(bool(mapping[i*3]) for i in range(92)),
                limitations=['Timer setup is modeled, not timing.', 'Synthetic initialized RAM/configuration; no complete boot/ADC/USB.',
                 'A8 changes live baseline and diverts ordinary key processing until A9; crash between commands remains hazardous.',
                 'A0 is event serialization, not atomic multi-key snapshot; service scheduling/USB losses need separate review.',
                 'Zero-length06 reloads stored configuration in this exact image; this is not proof for other firmware versions.'])
    a.output.write_text(json.dumps(result,indent=2),encoding='utf8')
    print('MIX87_REPLAY=PASS cases='+str(cases)+'; A8/A9 limited volatile path, normal debug writes flash')
if __name__=='__main__':main()
