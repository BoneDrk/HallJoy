import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from research_reference_checks import verify_record


class ReferenceChecks(unittest.TestCase):
    def test_absent_private_source_is_explicit_but_public_tampering_fails(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'map.h').write_bytes(b'original\n')
            record = [dict(path='map.h', private=False, sha256=hashlib.sha256(b'original\n').hexdigest()),
                      dict(path='vendor.js', private=True, sha256=hashlib.sha256(b'vendor').hexdigest())]
            self.assertEqual(verify_record(root, record), ['vendor.js'])
            (root / 'map.h').write_bytes(b'changed\n')
            with self.assertRaises(ValueError):
                verify_record(root, record)

    def test_present_private_evidence_must_match_even_with_missing_neighbor(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'vendor.js').write_bytes(b'changed')
            with self.assertRaises(ValueError):
                verify_record(root, [dict(path='missing.js', private=True, sha256='x'),
                                     dict(path='vendor.js', private=True, sha256='wrong')])

    def test_path_escape_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError):
                verify_record(Path(temp), [dict(path='../outside', private=True, sha256='x')])


if __name__ == '__main__':
    unittest.main()
