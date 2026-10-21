"""Build the article Makefile only in an isolated copy, without fetching anything."""
import argparse
from pathlib import Path
import shutil
import tempfile
from process import run


def check(source):
    with tempfile.TemporaryDirectory(prefix='cpp-make-') as directory:
        work = Path(directory) / 'project'
        shutil.copytree(source, work)
        result = run(['make', '--no-print-directory', '-j2'], cwd=work, timeout=40)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        result = run([str(work / 'my_program')], cwd=work)
        if result.returncode or result.stdout != 'product=42\n' or result.stderr:
            raise AssertionError(result)
    print('PASS article Makefile with labelled supporting sources')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    check(parser.parse_args().source)
