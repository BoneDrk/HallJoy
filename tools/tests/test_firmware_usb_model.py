import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from firmware_replay import thumb_machine, run_until
from firmware_usb_model import UsbSubmitModel
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_LR

BASE=0x08008000
READY=0x20000000


class UsbModelTests(unittest.TestCase):
    def test_ready_transitions_on_actual_firmware_reads(self):
        u=thumb_machine(bytes.fromhex('087800bf'),BASE) # ldrb r0,[r1]
        model=UsbSubmitModel(u,BASE+100,READY,busy_reads=2).install()
        try:
            values=[]
            for _ in range(4):
                u.reg_write(UC_ARM_REG_R1,READY)
                run_until(u,BASE,{BASE+2})
                values.append(u.reg_read(UC_ARM_REG_R0))
            self.assertEqual(values,[0,0,1,1])
        finally: model.close()

    def test_submission_acceptance_is_distinct_from_attempt(self):
        u=thumb_machine(bytes.fromhex('00bf00bf'),BASE)
        model=UsbSubmitModel(u,BASE,READY,fail_calls=(2,)).install()
        u.mem_write(READY+32,bytes.fromhex('070040'))
        try:
            returned=[]
            for _ in range(3):
                for reg,val in [(UC_ARM_REG_R0,0x82),(UC_ARM_REG_R1,READY+32),
                                (UC_ARM_REG_R2,3),(UC_ARM_REG_LR,(BASE+2)|1)]:u.reg_write(reg,val)
                run_until(u,BASE,{BASE+2})
                returned.append(u.reg_read(UC_ARM_REG_R0))
            self.assertEqual(returned,[0,1,0])
            self.assertEqual([e['accepted'] for e in model.events],[True,False,True])
        finally:model.close()

    def test_unknown_report_does_not_succeed_silently(self):
        u=thumb_machine(bytes.fromhex('00bf00bf'),BASE)
        model=UsbSubmitModel(u,BASE,READY).install()
        try:
            with self.assertRaises(ValueError):run_until(u,BASE,{BASE+2})
        finally:model.close()

    def test_invalid_busy_count_rejected(self):
        with self.assertRaises(ValueError):UsbSubmitModel(None,0,0,busy_reads=-1)


if __name__=='__main__':unittest.main()
