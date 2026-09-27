"""Targeted MAD68 command/branch/USB-fault replay, with explicit synthetic boundaries."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
from firmware_replay import run_until, ReplayError
from firmware_usb_model import UsbSubmitModel
from mad68_v2_dual_firmware_audit import VERSIONS, BASE, machine
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R2, UC_ARM_REG_R8, UC_ARM_REG_R10, UC_ARM_REG_PC


def load(cfg, directory):
    package=(directory/cfg['file']).read_bytes()
    if hashlib.sha256(package).hexdigest()!=cfg['sha']: raise ValueError('firmware hash mismatch')
    return (bytes(x ^ ((0xaa+i)&255) for i,x in enumerate(package[0x391e80:]))
            if cfg['file'].endswith('.exe') else package)


def scenario(version, cfg, data, name, failures=(), busy=0):
    u=machine(data)
    handler=BASE+cfg['table']+2*struct.unpack_from('<H',data,cfg['table']+(0x36-0x10)*2)[0]
    def mode(enabled):
        u.reg_write(UC_ARM_REG_R8,0x2001d000)
        u.mem_write(0x2001d008,bytes([enabled]))
        for i,pointer in enumerate(cfg['pointers']):
            u.mem_write(pointer,struct.pack('<I',0x2001c000+16*i))
        run_until(u,handler,{BASE+cfg['tail']})
        actual=u.mem_read(cfg['mode'],1)[0]
        if bool(actual & 2)!=bool(enabled): raise ValueError('mode command did not set requested bit')
        return actual
    mode(1)
    newer=version=='1.09'
    report=0x20008cc4 if newer else 0x20008ab4
    pointer=0x20004240 if newer else 0x20004034
    state=0x20006d44 if newer else 0x20006b34
    submit=BASE+(0x15126 if newer else 0x14d0a)
    u.mem_write(pointer,struct.pack('<I',report))
    u.mem_write(state+0x43c,struct.pack('<I',0x2001b000))
    usb=UsbSubmitModel(u,submit,0x2001b048,fail_calls=failures,busy_reads=busy).install()
    rows=[]
    def scan(depth, resume=False):
        if not resume:
            # Explicit bridge: sampled mode RAM is supplied as the register input
            # at the post-ADC boundary. The omitted caller/scan scheduler is NOT run.
            u.reg_write(UC_ARM_REG_R0,u.mem_read(cfg['mode'],1)[0])
            u.reg_write(UC_ARM_REG_R2,depth)
            u.reg_write(UC_ARM_REG_R10,0)
        first=len(usb.events)
        try:
            stop=run_until(u,u.reg_read(UC_ARM_REG_PC) if resume else BASE+cfg['normalize'],
                {BASE+cfg['normal']},budget=10000,
                predicate=lambda m,a,n: bytes(m.mem_read(a,2))==b'\x07\xb0')
            return dict(depth=depth, ordinary_branch=stop==BASE+cfg['normal'],
                        submissions=usb.events[first:], completed=True)
        except ReplayError as exc:
            return dict(depth=depth, completed=False, submissions=usb.events[first:], fault=exc.diagnostic)
    try:
        rows.append(scan(1500))
        blocked=not rows[-1]['completed']
        resumed=None
        if blocked:
            usb.busy_reads=0
            resumed=scan(1500,resume=True)
            if not resumed['completed']: raise ValueError('ready recovery did not reach return boundary')
        for depth in [0]*40: rows.append(scan(depth))
        disabled=mode(0)
        ordinary=scan(1500)
        enabled=mode(1)
        reenabled=scan(1500)
        return dict(name=name,mode_off=disabled,mode_on=enabled,
                    busy_reads=usb.reads,blocked=blocked,resumed=resumed,
                    passes=rows,after_disable=ordinary,after_reenable=reenabled)
    finally: usb.close()


def run(directory):
    results=[]
    for version,cfg in VERSIONS.items():
        data=load(cfg,directory)
        scenarios=[scenario(version,cfg,data,'baseline')]
        for n in range(1,5): scenarios.append(scenario(version,cfg,data,f'fail_submit_{n}',(n,)))
        scenarios.append(scenario(version,cfg,data,'ready_after_12_reads',busy=12))
        scenarios.append(scenario(version,cfg,data,'permanent_busy_then_resume',busy=None))
        results.append(dict(version=version,sha256=cfg['sha'],scenarios=scenarios))
    return results


def summary(results):
    return [dict(version=v['version'],scenarios=[dict(name=s['name'],blocked=s['blocked'],
        resumed=None if s['resumed'] is None else s['resumed']['completed'],
        release_accepted=[e['payload'] for p in s['passes'][1:] for e in p['submissions'] if e['accepted']],
        ordinary_branch_after_disable=s['after_disable'].get('ordinary_branch'),
        reports_after_reenable=len(s['after_reenable']['submissions'])) for s in v['scenarios']]) for v in results]


def validate_results(results):
    if {v['version'] for v in results} != set(VERSIONS):
        raise ValueError('incomplete version coverage')
    for version in results:
        if len(version['scenarios']) != 7: raise ValueError('incomplete fault coverage')
        for s in version['scenarios']:
            busy=s['name']=='permanent_busy_then_resume'
            if s['blocked'] != busy: raise ValueError('unexpected blocked state')
            if busy and (s['passes'][0]['fault']['reason']!='boundary_not_reached'
                         or not s['resumed']['completed']):
                raise ValueError('busy loop did not recover at the modeled boundary')
            if not all(p['completed'] for p in s['passes'][1:]): raise ValueError('release branch fault')
            if s['after_disable'].get('ordinary_branch') is not True:
                raise ValueError('disable did not restore ordinary branch entry')
            if len(s['after_reenable']['submissions']) != 2: raise ValueError('reenable failed')
            accepted=[e for p in s['passes'][1:] for e in p['submissions'] if e['accepted']]
            expected=1 if s['name'] in ('fail_submit_3','fail_submit_4') else 2
            if len(accepted)!=expected or any(e['payload']!='070040' for e in accepted):
                raise ValueError('release transport behavior changed; review raw evidence')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('inputs',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args()
    if not __debug__: p.error('optimized Python is not allowed')
    a.output.mkdir(parents=True,exist_ok=False)
    sources=['mad68_usb_lifecycle_audit.py','mad68_v2_dual_firmware_audit.py','firmware_replay.py','firmware_usb_model.py']
    assets={}
    for name in sources:
        data=(Path(__file__).parent/name).read_bytes()
        (a.output/name).write_bytes(data);assets[name]=hashlib.sha256(data).hexdigest()
    for cfg in VERSIONS.values():
        data=(a.inputs/cfg['file']).read_bytes()
        if hashlib.sha256(data).hexdigest()!=cfg['sha']: raise ValueError('input changed')
        (a.output/cfg['file']).write_bytes(data);assets[cfg['file']]=cfg['sha']
    # Execute snapshotted input bytes; source is checked again before publishing results.
    result=run(a.output)
    validate_results(result)
    for name in sources:
        if hashlib.sha256((Path(__file__).parent/name).read_bytes()).hexdigest()!=assets[name]:
            raise ValueError('producer changed during execution; discard result')
    for name,value in [('raw.json',result),('summary.json',summary(result))]:
        data=json.dumps(value,indent=2).encode();(a.output/name).write_bytes(data)
        assets[name]=hashlib.sha256(data).hexdigest()
    manifest=dict(schema=1,kind="mad68-usb-lifecycle",execution="expectations-reproduced-not-support",assets=assets,scope='Command36 and post-ADC branch joined with explicit RAM-to-register bridge; synthetic USB ready/return values.',
        limitations=['No physical USB or scheduler/interrupts','Ordinary processing entry observed, not typing HID delivery',
                     'Nonzero submit return is a fault hypothesis, not measured controller behavior','40 subsequent release passes only; no global USB retry scheduler'])
    (a.output/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
    print(json.dumps(summary(result),indent=2))


if __name__=='__main__': main()
