import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_publication_inputs import violations


class PublicationTests(unittest.TestCase):
    def test_known_payload_and_renamed_vendor_payload_blocked(self):
        self.assertEqual(len(violations(['docs/research/known.c', 'docs/new/FIRMWARE.BIN'], {'docs/research/known.c'})), 2)

    def test_new_archives_and_copied_source_require_review(self):
        self.assertEqual(len(violations(['docs/research/new.rs', 'docs/firmware/new.zip'], set())), 2)

    def test_private_and_traversal_blocked(self):
        self.assertEqual(len(violations(['.local/evidence.json', '../secret', 'docs\\secret'], set())), 3)

    def test_project_sources_and_factual_catalogs_allowed(self):
        self.assertEqual(violations(['src/main.cpp', 'docs/research/profiles.json', 'firmware/keychron_k4_he/base.patch'], set()), [])


if __name__ == '__main__':
    unittest.main()
