"""Strict Thumb branch execution; extend peripherals explicitly, never auto-map faults."""
from collections import deque
from unicorn import (Uc, UcError, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE,
                     UC_HOOK_MEM_INVALID, UC_PROT_READ, UC_PROT_WRITE, UC_PROT_EXEC)
from unicorn.arm_const import UC_ARM_REG_PC, UC_ARM_REG_SP, UC_ARM_REG_LR


class ReplayError(RuntimeError):
    def __init__(self, diagnostic):
        self.diagnostic = diagnostic
        super().__init__(str(diagnostic))


def thumb_machine(data, base, ram_base=0x20000000, ram_size=0x20000):
    if base % 4096 or ram_base % 4096 or ram_size % 4096 or not data:
        raise ValueError('page-aligned code/RAM and nonempty image required')
    u = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    u.mem_map(base, (len(data) + 4095) & ~4095, UC_PROT_READ | UC_PROT_EXEC)
    u.mem_write(base, data)
    u.mem_map(ram_base, ram_size, UC_PROT_READ | UC_PROT_WRITE)
    u.reg_write(UC_ARM_REG_SP, ram_base + ram_size - 0x1000)
    u.reg_write(UC_ARM_REG_LR, base | 1)
    u.replay_bounds = (base, base + len(data))
    return u


def run_until(u, entry, stops, *, budget=100000, timeout_us=2000000, predicate=None):
    """Bounded run with exact stops, last PCs and unmapped/protected access detail.

    A predicate is an explicitly modeled branch boundary, not automatic success.
    Existing capture hooks may model known calls, but must not stop execution.
    """
    if budget <= 0 or timeout_us <= 0:
        raise ValueError('positive execution bounds required')
    recent, reached, faults = deque(maxlen=24), [], []
    instructions = 0

    def code(machine, address, size, _):
        nonlocal instructions
        instructions += 1
        recent.append(hex(address))
        if address in stops or (predicate and predicate(machine, address, size)):
            reached.append(address)
            machine.emu_stop()
        elif not (u.replay_bounds[0] <= address < address + size <= u.replay_bounds[1]):
            faults.append(dict(kind='execution_outside_image', address=hex(address)))
            machine.emu_stop()

    def invalid(machine, access, address, size, value, _):
        faults.append(dict(kind='unmodeled_or_protected_memory', access=access,
                           address=hex(address), size=size, value=value))
        return False

    hooks = [u.hook_add(UC_HOOK_CODE, code), u.hook_add(UC_HOOK_MEM_INVALID, invalid)]
    error = None
    try:
        u.emu_start(entry | 1, 0, timeout=timeout_us, count=budget)
    except UcError as exc:
        error = str(exc)
    finally:
        for hook in hooks: u.hook_del(hook)
    if faults or error or not reached:
        raise ReplayError(dict(reason='execution_fault' if faults or error else 'boundary_not_reached',
                               entry=hex(entry), pc=hex(u.reg_read(UC_ARM_REG_PC)),
                               instructions=instructions, budget=budget, timeout_us=timeout_us,
                               last_pcs=list(recent), faults=faults, engine_error=error))
    return reached[0]
