"""Check a separately labelled intentional compile error, never execute it."""
import argparse
from pathlib import Path
import re
from process import run


def check(compiler, source, standard, diagnostic):
    result = run([compiler, f'-std=c++{standard}', '-fsyntax-only', str(source)], timeout=30)
    compare(result, diagnostic)
    print(f'PASS rejection {source.name}: {diagnostic}')


def compare(result, diagnostic):
    if result.returncode != 1 or not re.search(diagnostic, result.stdout + result.stderr, re.I):
        raise AssertionError(f'Not the expected rejection:\n{result.stdout}{result.stderr}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--standard', required=True, type=int)
    parser.add_argument('--diagnostic', required=True)
    args = parser.parse_args()
    check(args.compiler, args.source, args.standard, args.diagnostic)
