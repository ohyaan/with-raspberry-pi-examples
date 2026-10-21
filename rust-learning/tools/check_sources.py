"""Check article-derived bytes locally and optionally against the site checkout."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def check(site_source=None):
    entries = json.loads((ROOT / 'article-sources.json').read_text())
    for name, digest in entries.items():
        path = ROOT / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise AssertionError(f'Missing or changed article-derived file: {name}')
    print(f'PASS {len(entries)} article-derived files: exact hashes')
    if site_source:
        subprocess.run([sys.executable, '-m', 'scripts.rust_example_sources',
                        '--check', str(ROOT.parent)], cwd=site_source, check=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site-source', type=Path)
    check(parser.parse_args().site_source)
