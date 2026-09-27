import hashlib
import sys
from pathlib import Path
import tempfile
import unittest
import zipfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from audit_distribution_inventory import inventory


class InventoryTests(unittest.TestCase):
    def test_truncated_remote_tree_is_not_a_complete_audit(self):
        with self.assertRaises(ValueError):inventory(Path('.'),dict(truncated=True,tree=[]))

    def test_outside_path_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError):
                inventory(Path(temp),dict(truncated=False,tree=[dict(type='blob',path='../outside',sha='x')]))

    def test_members_and_hash_mismatch_are_visible_without_clearance(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);p=root/'docs'/'research'/'sample.zip';p.parent.mkdir(parents=True)
            with zipfile.ZipFile(p,'x') as z:z.writestr('vendor.bin',b'vendor image')
            result=inventory(root,dict(truncated=False,sha='snapshot',tree=[
                dict(type='blob',path='docs/research/sample.zip',sha='not-current')]))
            row=result['inventory'][0]
            self.assertFalse(result['legal_clearance'])
            self.assertFalse(row['local_matches_published'])
            self.assertEqual(row['archive_members'][0]['path'],'vendor.bin')
            self.assertEqual(row['category'],'archive-review')


if __name__=='__main__':unittest.main()
