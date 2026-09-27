"""Explicit synchronous submit/ready-byte model; not a USB controller emulator."""
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_READ
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_PC, UC_ARM_REG_LR


class UsbSubmitModel:
    def __init__(self, machine, submit, ready, *, fail_calls=(), busy_reads=0,
                 endpoint=0x82, length=3, failure_result=1):
        if busy_reads is not None and busy_reads < 0: raise ValueError('negative busy count')
        self.u, self.submit, self.ready = machine, submit, ready
        self.fail_calls, self.busy_reads = set(fail_calls), busy_reads
        self.endpoint, self.length, self.failure_result = endpoint, length, failure_result
        self.reads, self.events, self.hooks = 0, [], []

    def install(self):
        if self.hooks: raise ValueError('already installed')
        self.hooks = [self.u.hook_add(UC_HOOK_CODE, self._code),
                      self.u.hook_add(UC_HOOK_MEM_READ, self._read, begin=self.ready, end=self.ready)]
        return self

    def close(self):
        for h in self.hooks: self.u.hook_del(h)
        self.hooks.clear()

    def _read(self, u, access, address, size, value, context):
        self.reads += 1
        ready = self.busy_reads is not None and self.reads > self.busy_reads
        u.mem_write(self.ready, bytes([int(ready)]))

    def _code(self, u, address, size, context):
        if address != self.submit: return
        ep, pointer, length = (u.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2))
        if ep != self.endpoint or length != self.length:
            raise ValueError('submit outside reviewed endpoint/length contract')
        number = len(self.events) + 1
        accepted = number not in self.fail_calls
        self.events.append(dict(call=number, endpoint=ep,
                                payload=bytes(u.mem_read(pointer,length)).hex(), accepted=accepted))
        u.reg_write(UC_ARM_REG_R0, 0 if accepted else self.failure_result)
        u.reg_write(UC_ARM_REG_PC, u.reg_read(UC_ARM_REG_LR))
