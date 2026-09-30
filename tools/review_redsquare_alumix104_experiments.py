"""Offline stock104 experiments: stateful simulation lease and candidate echoes.
No hardware access; missing code/peripherals remain failures, never stubs.
"""
import argparse
import hashlib
import json
from pathlib import Path
from review_redsquare_alumix104_capture import HASH, ENTRY, RETURN, RX, TX, FLAGS
from firmware_replay import thumb_machine, run_until
from unicorn import UC_HOOK_MEM_WRITE
from unicorn.arm_const import UC_ARM_REG_SP


def machine(data, seed):
    u = thumb_machine(data, 0xc000)
    u.mem_write(0x20000000, bytes((i * 17 + seed) & 255 for i in range(0x20000)))
    u.mem_write(0x200000ac, b"\x00")  # Live RGB cleanup requires missing code.
    return u


def dispatch(u, command, payload=b"", offset=0):
    if len(payload) > 56:
        raise ValueError("ordinary 64-byte payload only")
    packet = bytearray(64)
    packet[:5] = bytes((0xaa, command, len(payload), offset & 255, offset >> 8))
    packet[6] = 1
    packet[8:8 + len(payload)] = payload
    u.mem_write(RX, bytes(packet))
    u.mem_write(FLAGS + 4, b"\x01")
    u.mem_write(FLAGS + 3, b"\x00")
    u.reg_write(UC_ARM_REG_SP, 0x2001f000)
    writes = []
    hook = u.hook_add(UC_HOOK_MEM_WRITE, lambda m, a, addr, n, v, x: writes.append((addr, n)))
    try:
        stop = run_until(u, ENTRY, {RETURN})
    finally:
        u.hook_del(hook)
    # Exclude known reply, pending flags, simulation flag and stack only.
    unexpected = [(a, n) for a, n in writes if not (
        TX <= a and a + n <= TX + 64 or
        FLAGS + 2 <= a and a + n <= FLAGS + 5 or
        0x2001ef00 <= a and a + n <= 0x2001f000)]
    if unexpected:
        raise RuntimeError(f"unexpected writes: {unexpected}")
    return bytes(u.mem_read(TX, 64)), bytes(packet), stop


def experiments(data):
    lifecycle = []
    query_counts = {f"0x{command:02x}": 0 for command in (0x60, 0x68, 0x69, 0x6a)}
    for seed in (0, 1, 85, 170, 255):
        for initial in (0, 1):
            for calibration in (0, 1):
                u = machine(data, seed)
                u.mem_write(FLAGS + 1, bytes((calibration, initial)))
                steps = []
                for command in (0x67, 0x67, 0x66, 0x66, 0x67, 0x66, 0x67):
                    reply, packet, stop = dispatch(u, command)
                    observed = u.mem_read(FLAGS + 2, 1)[0]
                    if observed != (command == 0x66):
                        raise RuntimeError("simulation flag transition mismatch")
                    if u.mem_read(FLAGS + 1, 1)[0] != calibration:
                        raise RuntimeError("calibration flag modified")
                    if reply != b"\x55" + packet[1:]:
                        raise RuntimeError("mode ACK mismatch")
                    steps.append(dict(command=hex(command), simulation=observed))
                lifecycle.append(dict(seed=seed, initial=initial, calibration=calibration, steps=steps))
        for simulation in (0, 1):
            u = machine(data, seed)
            u.mem_write(FLAGS + 1, bytes((0, simulation)))
            for command in (0x60, 0x68, 0x69, 0x6a):
                for count in range(1, 8):
                    for offset in (0, 24, 56, 112):
                        payload = bytes((seed + i * 13) & 255 for i in range(count * 8))
                        reply, packet, stop = dispatch(u, command, payload, offset)
                        if reply != b"\x55" + packet[1:]:
                            raise RuntimeError(f"{command:02x} is not a pure echo in this scenario")
                        if u.mem_read(FLAGS + 2, 1)[0] != simulation:
                            raise RuntimeError("query modified simulation")
                        query_counts[f"0x{command:02x}"] += 1
    return dict(status="PASS", lifecycle_scenarios=len(lifecycle),
                lifecycle_commands=sum(len(r["steps"]) for r in lifecycle),
                candidate_echo_cases=query_counts, lifecycle=lifecycle,
                findings={
                    "mode": "66/67 are idempotent RAM toggles in captured dispatcher, including already-on state; 67 clears flag without changing calibration flag",
                    "query": "60/68/69/6A echo tested 1..7-record payloads in both simulation states within captured dispatcher; this does not rule out asynchronous producers or unexecuted lower code",
                    "scope": "Synthetic RAM seeds, RGB disabled; no missing code fabricated. This does NOT execute scanner, USB producer, normal typing, disconnect recovery or physical timing.",
                    "next_hardware_candidate": "Official 66/67 stream, bounded lease with cleanup; typing/multikey/range/release remain unknown. Never use 64/65 calibration or settings writes."})


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("capture", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    data = args.capture.read_bytes()
    if hashlib.sha256(data).hexdigest() != HASH:
        raise ValueError("wrong exact104 capture")
    if args.output.exists():
        raise FileExistsError(args.output)
    result = experiments(data)
    result["capture_sha256"] = HASH
    result["source_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    with args.output.open("x", encoding="utf-8") as f:
        json.dump(result, f, indent=2)
        f.write("\n")
    print(json.dumps({k: v for k, v in result.items() if k != "lifecycle"}, indent=2))


if __name__ == "__main__":
    main()
