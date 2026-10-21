"""Check existing article programs with finite, offline operations."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
LESSONS = json.loads((ROOT / 'catalog.json').read_text())


def run(command, cwd=ROOT, *, expected=None, failure=None, timeout=180):
    process = subprocess.Popen(command, cwd=cwd, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True, start_new_session=True,
                               env={**os.environ, 'CARGO_BUILD_JOBS': '2'})
    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        # A Cargo timeout must not leave rustc or a test binary running.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.communicate()
        raise
    result = subprocess.CompletedProcess(command, process.returncode, stdout, stderr)
    output = result.stdout + result.stderr
    if failure is not None:
        # Reject build-system errors and crashes as well as accidental success.
        if result.returncode not in (1, 101) or failure not in output:
            raise RuntimeError(f'Wrong failure: {command}\n{output}')
    elif result.returncode:
        raise RuntimeError(f'Failed: {command}\n{output}')
    if expected is not None and result.stdout != expected:
        raise AssertionError(f'Output mismatch: {command}\nexpected={expected!r}\nactual={result.stdout!r}')
    return result


def verify(failures=False, cpp=False):
    print(run(['rustc', '--version']).stdout.strip(), flush=True)
    print(run(['cargo', '--version']).stdout.strip(), flush=True)
    macro = next(r for r in LESSONS if r['number'] == 24)
    for release in (False, True):
        profile = ['--release'] if release else []
        run(['cargo', 'test', '--workspace', '--offline', '--jobs', '2', *profile])
        run(['cargo', 'test', '--workspace', '--offline', '--jobs', '2', *profile],
            ROOT / 'lessons' / macro['slug'])
        for lesson in LESSONS:
            path = ROOT / 'lessons' / lesson['slug']
            run(['cargo', 'run', '--offline', '--quiet', *profile, '--', *lesson['args']],
                path, expected=(path / 'expected.txt').read_text())
            if lesson['number'] == 25:
                for features in (['--no-default-features'], ['--features', 'detailed'], ['--all-features']):
                    run(['cargo', 'test', '--offline', '--jobs', '2', *profile, *features], path)
                    expected = 'expected.txt' if features == ['--no-default-features'] else 'expected-detailed.txt'
                    run(['cargo', 'run', '--offline', '--quiet', *profile, *features], path,
                        expected=(path / expected).read_text())
        print('PASS 41 working lessons: ' + ('release' if release else 'debug') + ', exact outputs and feature matrix', flush=True)
    run(['cargo', 'fmt', '--all', '--check'])
    run(['cargo', 'fmt', '--all', '--check'], ROOT / 'lessons' / macro['slug'])
    if failures:
        count = 0
        for lesson in LESSONS:
            if not lesson['diagnostic']:
                continue
            path = ROOT / 'failures' / lesson['slug']
            if lesson['failure_kind'] == 'test':
                command = ['cargo', 'test', '--offline', '--test', 'broken_contract']
            else:
                command = ['cargo', 'check', '--offline']
            run(command, path, failure=lesson['diagnostic'])
            count += 1
        print(f'PASS {count} intentional Rust failures: expected rejection reasons', flush=True)
    if cpp:
        with tempfile.TemporaryDirectory(prefix='rust-cpp-', dir=ROOT / 'target') as directory:
            directory = Path(directory)
            for lesson in LESSONS:
                if not lesson['cpp_diagnostic']:
                    continue
                path = ROOT / 'lessons' / lesson['slug']
                for level in (0, 2):
                    executable = directory / f"{lesson['number']}-O{level}"
                    run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Wpedantic',
                         f'-O{level}', 'comparison-cpp/main.cpp', '-o', str(executable)], path)
                    run([str(executable)], path, expected=(path / 'expected.txt').read_text())
                run(['g++', '-std=c++20', '-fsyntax-only', 'comparison-cpp/failure.cpp'], path,
                    failure=lesson['cpp_diagnostic'])
        print('PASS 12 C++ comparisons: O0/O2 outputs and intentional compile failures', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--failures', action='store_true')
    parser.add_argument('--cpp', action='store_true')
    args = parser.parse_args()
    verify(args.failures, args.cpp)
