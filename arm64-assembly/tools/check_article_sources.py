"""Check every named course source against a local site-source checkout."""
import argparse
from pathlib import Path
import re


def check(site_source):
    category = Path(__file__).resolve().parents[1]
    expected = {}
    for page in (site_source / 'docs/assembly').glob('*.md'):
        for code in re.findall(r'^```(?:asm|c|cpp)\n(.*?)^```', page.read_text(), re.M | re.S):
            match = re.match(r'(?://|/\*) file: ([a-zA-Z0-9_.]+)', code)
            if not match:
                raise AssertionError(f'Unnamed source in {page.name}')
            if match[1] in expected:
                raise AssertionError(f'Duplicate source: {match[1]}')
            expected[match[1]] = code.encode()
    actual = {p.name: p.read_bytes() for p in (category / 'app').rglob('*')
              if p.suffix in ('.S', '.c', '.cpp')}
    if not expected or expected != actual:
        missing = sorted(expected.keys() - actual.keys())
        extra = sorted(actual.keys() - expected.keys())
        changed = sorted(n for n in expected.keys() & actual.keys() if expected[n] != actual[n])
        raise AssertionError(f'missing={missing}; extra={extra}; changed={changed}')
    print(f'PASS: {len(expected)} article source files match byte for byte')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site-source', required=True, type=Path)
    check(parser.parse_args().site_source)
