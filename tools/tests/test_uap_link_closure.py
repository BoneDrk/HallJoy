from pathlib import Path
import sys
import unittest
import hashlib
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verify_uap_link_closure import inspect_map, REVIEWED_SOUP, verify_record


class ClosureTests(unittest.TestCase):
    def fixture(self):
        return 'main.o\n' + '\n'.join('soup:' + name for name in REVIEWED_SOUP)

    def test_record_binds_exact_binary_and_map(self):
        dll, mapping = b'MZtest', self.fixture().encode()
        record = dict(schema=1, abi=1, dll_sha256=hashlib.sha256(dll).hexdigest(),
                      link_map_sha256=hashlib.sha256(mapping).hexdigest(),
                      soup_objects=sorted(REVIEWED_SOUP), legacy_common_linked=False)
        verify_record(record, dll, mapping)
        for binary, link_map in ((dll + b'changed', mapping), (dll, mapping + b'\n')):
            with self.assertRaises(ValueError):
                verify_record(record, binary, link_map)

    def test_known_members(self):
        self.assertEqual(set(inspect_map(self.fixture())), REVIEWED_SOUP)

    def test_additional_code_needs_review(self):
        with self.assertRaises(ValueError):
            inspect_map(self.fixture() + '\nsoup:TinyPngOut.o')

    def test_rust_common_or_missing_members_rejected(self):
        for text in (self.fixture() + '\nwooting_analog_common:foo.obj', 'main.o\nsoup:hwHid.o'):
            with self.assertRaises(ValueError):
                inspect_map(text)


if __name__ == '__main__':
    unittest.main()
