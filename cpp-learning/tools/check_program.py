"""Compare an example's output in an isolated working directory."""
import argparse
import json
from pathlib import Path
import re
import tempfile

from process import run

ROOT = Path(__file__).resolve().parents[1]


def check(target, executable):
    spec = next(p for p in json.loads((ROOT / 'programs.json').read_text()) if p['target'] == target)
    with tempfile.TemporaryDirectory(prefix='cpp-course-') as directory:
        result = run([str(executable.resolve())], cwd=directory, stdin=spec['stdin'])
    compare(spec, result)
    print(f'PASS {target}: output and exit status')


def compare(spec, result):
    target = spec['target']
    if result.returncode or result.stderr:
        raise AssertionError(f'{target}: {result.returncode} {result.stderr}')
    expected = spec['expected']
    matches = bool(re.fullmatch(expected.removeprefix('regex:'), result.stdout)) if expected.startswith('regex:') else result.stdout == expected
    if not matches:
        raise AssertionError(f'{target}: expected {expected!r}, got {result.stdout!r}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--skip')
    parser.add_argument('--target')
    parser.add_argument('--executable', type=Path)
    args = parser.parse_args()
    if args.skip:
        print(f'SKIP {args.skip}: unavailable compiler/library combination, not a passed example')
        raise SystemExit(77)
    if not args.target or not args.executable:
        parser.error('--target and --executable are required unless --skip is used')
    check(args.target, args.executable)
