"""Full source-backed layout checks; run locally when recording public references."""
from pathlib import Path
import sys
import unittest
import layout_pipeline

ROOT = Path(__file__).resolve().parents[1]
BRANDS = ('Keychron', 'Lemokey', 'DrunkDeer', 'Aula', 'Redragon', 'Razer',
          'NuPhy', 'Wooting', 'IROK', 'MADLIONS', 'ATK', 'IPI', 'SayoDevice',
          'MonsGeek', 'EPOMAKER', 'Chilkey')


def main():
    suite = unittest.defaultTestLoader.discover(str(ROOT / 'tools/tests'), pattern='test_layout_pipeline.py')
    result = unittest.TextTestRunner().run(suite)
    if not result.wasSuccessful():
        raise RuntimeError('Source-backed layout tests failed')
    for brand in BRANDS:
        sys.argv = ['layout_pipeline.py', 'check', brand]
        if layout_pipeline.main() != 0:
            raise RuntimeError('Source-backed layout check failed: ' + brand)


if __name__ == '__main__':
    main()
