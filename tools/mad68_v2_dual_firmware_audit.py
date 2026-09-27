"""Offline, hash-pinned MAD68 V2 Dual firmware branch replay. Never opens HID.

Requires capstone and unicorn. Inputs are local copies of official packages;
this is selected ARM instruction replay, not a complete keyboard emulator.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from firmware_replay import thumb_machine, run_until

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,
                              UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R8, UC_ARM_REG_R9, UC_ARM_REG_R10,
                              UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC)

BASE = 0x08008000  # Reset vector 080081B5 resolves to the startup at file 01B4.
VERSIONS = {
    "1.07": dict(file="MK655-DoubleLight-V0107.exe",
                 sha="4a11126d207473c379a08b34a4ebe0543e25a2b41c599fca2b1b2be770bb3f74",
                 table=0x19fa, mode=0x20006a6a, pointers=(0x20003a68, 0x20003a64),
                 dispatch=0x19dc, rejected=0x28d4,
                 tail=0x305a, normalize=0x12a78, normalized=0x12b62,
                 normal=0x12af2, settings=0x20003648, descriptor=0x104c8),
    "1.09": dict(file="MK655-Dual-V0109.bin",
                 sha="066655ce6efc9bcca590fe7423a083b6b92ec8f42854d18e412c6afc697267cf",
                 table=0x1a1e, mode=0x20006c7a, pointers=(0x20003c74, 0x20003c70),
                 dispatch=0x1a00, rejected=0x2908,
                 tail=0x308e, normalize=0x12c90, normalized=0x12d7a,
                 normal=0x12d0a, settings=0x20003854, descriptor=0x106d8),
}

VERSIONS["1.06"] = dict(VERSIONS["1.07"], file="MK655-Dual-V1006-api.bin",
                        sha="8fb6cec0467c66cb2d02a831db5f0dd9b92ee5ae44ff57efce4c55a51ed37057")


def machine(data):
    return thumb_machine(data, BASE)



def audit(version, cfg, directory):
    packaged = (directory / cfg["file"]).read_bytes()
    assert hashlib.sha256(packaged).hexdigest() == cfg["sha"]
    if cfg["file"].endswith(".exe"):
        # Package overlay: 128-byte target name, then incrementing XOR key AA.
        data = bytes(v ^ ((0xaa + i) & 255)
                     for i, v in enumerate(packaged[0x391e80:]))
    else:
        data = packaged
    assert struct.unpack_from("<I", data, 4)[0] == BASE + 0x1b5
    desc = data[cfg["descriptor"]:cfg["descriptor"] + 18]
    assert desc[:2] == b"\x12\x01"
    assert struct.unpack_from("<HH", desc, 8) == (0x28e9, 0x3265)
    table = cfg["table"]
    routes = {c: BASE + table + 2 * struct.unpack_from("<H", data, table + (c - 0x10) * 2)[0]
              for c in range(0x10, 0xb2)}
    rejected = BASE + cfg["rejected"]
    boundaries = set(routes.values()) | {rejected}
    # Every possible opcode traverses the actual bounds check/jump table.
    # Stop before any command body, including calibration/flash/reset handlers.
    for c in range(256):
        u = machine(data)
        u.reg_write(UC_ARM_REG_R6, c)
        u.reg_write(UC_ARM_REG_R8, 0x2001d000)
        u.reg_write(UC_ARM_REG_R5, cfg["mode"] - 6)
        actual = run_until(u, BASE + cfg["dispatch"], boundaries)
        assert actual == routes.get(c, rejected)
    handler = BASE + table + 2 * struct.unpack_from("<H", data, table + (0x36 - 0x10) * 2)[0]
    command_results = []
    for enabled in (0, 1):
        u = machine(data)
        packet = 0x2001d000
        u.reg_write(UC_ARM_REG_R8, packet)
        u.mem_write(packet + 8, bytes([enabled]))
        u.mem_write(cfg["mode"], b"\xa5")
        for i, pointer in enumerate(cfg["pointers"]):
            u.mem_write(pointer, struct.pack("<I", 0x2001c000 + 16 * i))
        writes = []
        u.hook_add(UC_HOOK_MEM_WRITE, lambda _, access, a, n, v, ctx: writes.append((a, n)))
        run_until(u, handler, {BASE + cfg["tail"]})
        assert u.mem_read(packet + 8, 1)[0] == enabled  # reply retains requested mode
        mode = u.mem_read(cfg["mode"], 1)[0]
        assert mode == (0xa5 | (enabled << 1))
        assert all(0x20000000 <= a < a + n <= 0x20020000 for a, n in writes)
        command_results.append(dict(enable=enabled, mode=mode, ram_writes=len(writes)))
    samples = []
    for setting in (0, 1):
        for key in range(144):
            for depth in (0, 100, 199, 200, 259, 260, 399, 400, 459, 460, 1500, 3070, 3071, 3170, 3171, 3249, 3250, 4095):
                u = machine(data)
                u.reg_write(UC_ARM_REG_R0, 2)
                u.reg_write(UC_ARM_REG_R2, depth)
                u.reg_write(UC_ARM_REG_R10, key)
                u.mem_write(cfg["settings"] + 0x3d, bytes([setting]))
                run_until(u, BASE + cfg["normalize"], {BASE + cfg["normalized"]})
                output = u.reg_read(UC_ARM_REG_R6)
                special = key in (15, 30, 44, 61, 55, 42, 13)
                threshold = (260 if special else 200) + 200 * setting
                expected = min(depth, 3250)
                if special and expected > (3070 if setting else 3170): expected = 3250
                if depth < threshold: expected = 0
                assert output == expected, (version, setting, key, depth, output, expected)
                samples.append(dict(setting=setting, key=key, input=depth, output=output))
    # With simulation off the same branch reaches ordinary processing instead.
    u = machine(data)
    u.reg_write(UC_ARM_REG_R0, 0)
    u.reg_write(UC_ARM_REG_R2, 1500)
    run_until(u, BASE + cfg["normalize"], {BASE + cfg["normal"]})
    # Execute the real report-producing branch with USB submission replaced by
    # capture/return. The RAM descriptor's ready byte is synthetic, not USB timing.
    report = 0x20008ab4 if version != "1.09" else 0x20008cc4
    pointer = 0x20004034 if version != "1.09" else 0x20004240
    usb_state = 0x20006b34 if version != "1.09" else 0x20006d44
    submit = BASE + (0x14d0a if version != "1.09" else 0x15126)

    def replay_depths(depths):
        u = machine(data)
        u.mem_write(pointer, struct.pack("<I", report))
        u.mem_write(usb_state + 0x43c, struct.pack("<I", 0x2001b000))
        u.mem_write(0x2001b048, b"\x01")
        captured = []
        def stream_hook(machine, address, size, _):
            if address == submit:
                assert machine.reg_read(UC_ARM_REG_R0) == 0x82
                n = machine.reg_read(UC_ARM_REG_R2)
                assert n == 3, "Unexpected report length; review before modeling"
                captured.append(bytes(machine.mem_read(machine.reg_read(UC_ARM_REG_R1), n)).hex())
                machine.reg_write(UC_ARM_REG_R0, 0)
                machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))
        u.hook_add(UC_HOOK_CODE, stream_hook)
        rows = []
        for depth in depths:
            captured.clear()
            u.reg_write(UC_ARM_REG_R0, 2)
            u.reg_write(UC_ARM_REG_R2, depth)
            u.reg_write(UC_ARM_REG_R10, 0)
            run_until(u, BASE + cfg["normalize"], set(),
                      predicate=lambda m,a,n: bytes(m.mem_read(a,2)) == b"\x07\xb0")
            rows.append(dict(input=depth, reports=list(captured)))
        return rows

    # RAM persists within a scenario; scenarios start from separate synthetic state.
    stream = replay_depths([1500, 1510] + [1510] * 31 + [0])
    waveforms = [dict(name=name, passes=replay_depths(depths)) for name, depths in [
        ("slow_press_release", list(range(0,3301,25)) + list(range(3300,-1,-25))),
        ("shallow_hold", [0]*2 + [199]*40 + [200]*40 + [0]*40),
        ("threshold_jitter", [199,200]*40 + [0]*40),
        ("small_changes_hold_release", [1500]*2 + [1510]*40 + [1500]*40 + [0]*40),
        ("fast_taps", [0,1500,0,3250]*20 + [0]*40),
    ]]
    assert stream[0]["reports"] == ["07005c", "070057"]
    assert stream[-1]["reports"] == ["070040", "070040"]
    assert not stream[1]["reports"]
    assert [i for i, row in enumerate(stream) if row["reports"]] == [0, 32, 33]
    assert stream[32]["reports"] == ["070066", "070057"]
    # Undocumented command82: binary factory event, not analog depth.
    old_family = version != "1.09"
    entry, send, done, ordinary = ((0x7e64, 0x8ae4, 0x7db0, 0x7ed6) if old_family
                                   else (0x7ee0, 0x8bfc, 0x7e2c, 0x7f52))
    factory_table = data[0x11680:0x1168c] if old_family else data[0x11890:0x1189c]
    assert factory_table == bytes.fromhex("3f0140ffffffffffffffffff")
    factory = []
    for key in (0, 0x3f):
        for state in (0, 1, 127, 255):
            u = machine(data)
            u.reg_write(UC_ARM_REG_R4, 0x2001a000)
            u.reg_write(UC_ARM_REG_R9, 0x2001a010)
            u.reg_write(UC_ARM_REG_R8, 0x2001a020)
            u.mem_write(0x2001a010, bytes([key]))
            u.mem_write(0x2001a020, bytes([state]))
            u.mem_write(cfg["mode"] - 2, b"\x40")
            u.mem_write(pointer, struct.pack("<I", report))
            u.mem_write(report, bytes.fromhex("070000"))
            captured_factory = []
            def factory_hook(machine, address, size, _):
                if address == BASE + send:
                    captured_factory.append(bytes(machine.mem_read(report, 3)).hex())
                    machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))
            u.hook_add(UC_HOOK_CODE, factory_hook)
            reached = run_until(u, BASE + entry, {BASE + done, BASE + ordinary})
            assert reached == BASE + (done if key == 0x3f else ordinary)
            assert captured_factory == (["070140" if state else "070040"] if key == 0x3f else [])
            factory.append(dict(key=key, state=state, reports=captured_factory,
                                ordinary_processing=key != 0x3f))
    get_report_cases = []
    if version == "1.09":
        # HID GET_REPORT (including Feature) returns without supplying any data.
        for interface in (0, 1):
            for report_type in (1, 2, 3):
                for report_id in (6, 7):
                    u = machine(data)
                    context, setup = 0x20018000, 0x20019000
                    u.reg_write(UC_ARM_REG_R0, context)
                    u.reg_write(UC_ARM_REG_R1, setup)
                    u.mem_write(setup, struct.pack("<BBHHH", 0xa1, 1,
                                (report_type << 8) | report_id, interface, 64))
                    u.mem_write(context + 0xd8, struct.pack("<I", 0x12345678))
                    u.mem_write(context + 0xe4, struct.pack("<I", 0xabcdef01))
                    run_until(u, BASE + 0x6024, {BASE})
                    assert bytes(u.mem_read(context + 0xd8, 4)) == struct.pack("<I", 0x12345678)
                    assert bytes(u.mem_read(context + 0xe4, 4)) == struct.pack("<I", 0xabcdef01)
                    get_report_cases.append(dict(interface=interface, report_type=report_type,
                                                 report_id=report_id, response_configured=False))
    return dict(version=version, sha256=cfg["sha"], usb="28E9:3265",
                hid_get_report_replay=get_report_cases,
                command82_factory_replay=factory,
                dispatch_opcodes_replayed=256,
                accepted_commands={f"{c:02X}": hex(a) for c, a in routes.items() if a != rejected},
                command36_address=hex(handler), command_replay=command_results,
                normal_branch_reached=True, simulation_samples=samples, stream_replay=stream, waveforms=waveforms)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--report-dir", type=Path, help="New directory for raw evidence and mandatory behavior assessment")
    args = parser.parse_args()
    if not __debug__:
        parser.error("Optimized Python disables assertions; use normal Python for this audit")
    results = [audit(v, c, args.directory) for v, c in VERSIONS.items()]
    if args.report_dir:
        from mad68_firmware_behavior import write_report
        write_report(args.report_dir, results, args.directory)
    else:
        print(json.dumps(results, indent=2))
