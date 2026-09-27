import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from firmware_replay import thumb_machine, run_until, ReplayError
from firmware_inspect import pointer, summarize_samples, waveform_summary
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1

BASE=0x08008000


class ReplayTests(unittest.TestCase):
    def test_real_thumb_and_reusable_boundaries(self):
        u=thumb_machine(bytes.fromhex('013000bf'), BASE)  # adds r0,1; nop
        run_until(u,BASE,{BASE+2})
        run_until(u,BASE,{BASE+2})
        self.assertEqual(u.reg_read(UC_ARM_REG_R0),2)

    def test_loop_budget_is_failure_with_trace(self):
        u=thumb_machine(bytes.fromhex('fee7'),BASE)  # b .
        with self.assertRaises(ReplayError) as caught:
            run_until(u,BASE,{BASE+10},budget=32)
        d=caught.exception.diagnostic
        self.assertEqual(d['reason'],'boundary_not_reached')
        self.assertEqual(d['instructions'],32)
        self.assertEqual(len(d['last_pcs']),24)

    def test_unknown_mmio_is_not_automapped(self):
        u=thumb_machine(bytes.fromhex('086800bf'),BASE)  # ldr r0,[r1]
        u.reg_write(UC_ARM_REG_R1,0x40000000)
        with self.assertRaises(ReplayError) as caught: run_until(u,BASE,{BASE+2})
        self.assertEqual(caught.exception.diagnostic['faults'][0]['address'],'0x40000000')
        # Failed hooks are removed; same machine can run a corrected reviewed setup.
        u.reg_write(UC_ARM_REG_R1,0x20000000)
        run_until(u,BASE,{BASE+2})

    def test_flash_write_denied_but_ram_write_works(self):
        u=thumb_machine(bytes.fromhex('086000bf'),BASE)  # str r0,[r1]
        u.reg_write(UC_ARM_REG_R0,123)
        u.reg_write(UC_ARM_REG_R1,BASE)
        with self.assertRaises(ReplayError): run_until(u,BASE,{BASE+2})
        u.reg_write(UC_ARM_REG_R1,0x20000000)
        run_until(u,BASE,{BASE+2})
        self.assertEqual(int.from_bytes(u.mem_read(0x20000000,4),'little'),123)

    def test_execution_in_page_padding_is_not_code(self):
        u=thumb_machine(bytes.fromhex('00bf'),BASE)
        with self.assertRaises(ReplayError): run_until(u,BASE,set())

    def test_no_unbounded_run(self):
        u=thumb_machine(bytes.fromhex('00bf'),BASE)
        for kwargs in ({'budget':0},{'timeout_us':0}):
            with self.assertRaises(ValueError): run_until(u,BASE,set(),**kwargs)


class InspectionTests(unittest.TestCase):
    def test_pair_decoder_flags_unknown_format(self):
        result = waveform_summary(dict(name='fixture', passes=[
            dict(reports=['07005c','070057']), dict(reports=[])]))
        self.assertEqual(result['final_reported_value'],1500)
        self.assertEqual(result['decoded_pairs'],1)
        result = waveform_summary(dict(name='fixture', passes=[dict(reports=['07005c'])]))
        self.assertEqual(result['malformed_passes'],1)
        self.assertIsNone(result['final_reported_value'])

    def test_pointer_retrieves_exact_evidence(self):
        self.assertEqual(pointer({'a/b':[{'~':42}]},'/a~1b/0/~0'),42)
        with self.assertRaises(KeyError): pointer({},'/missing')

    def test_grouping_preserves_differences_not_inferred_resolution(self):
        rows=[dict(setting=0,key=k,input=x,output=y)
              for k in (0,1) for x,y in ((0,0),(199,0),(200,200))]
        rows += [dict(setting=0,key=2,input=200,output=0)]
        groups=summarize_samples(rows)
        normal=next(g for g in groups if g['keys']==[0,1])
        self.assertEqual(normal['smallest_sampled_nonzero_input'],200)
        self.assertEqual(normal['observed_distinct_outputs'],2)
        self.assertEqual(len(groups),2)


if __name__=='__main__': unittest.main()
