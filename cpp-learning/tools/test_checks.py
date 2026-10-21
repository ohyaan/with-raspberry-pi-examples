from pathlib import Path
import subprocess
import sys
import unittest

from check_program import compare as compare_output
from check_failure import compare as compare_failure
from process import run


class CheckerTests(unittest.TestCase):
    def test_exact_and_full_regex_output(self):
        spec = {'target': 'fixture', 'expected': 'value=42\n'}
        compare_output(spec, subprocess.CompletedProcess([], 0, 'value=42\n', ''))
        with self.assertRaises(AssertionError):
            compare_output(spec, subprocess.CompletedProcess([], 0, 'value=41\n', ''))
        spec['expected'] = 'regex:value=[0-9]+\n'
        compare_output(spec, subprocess.CompletedProcess([], 0, 'value=42\n', ''))
        with self.assertRaises(AssertionError):
            compare_output(spec, subprocess.CompletedProcess([], 0, 'value=42\nextra', ''))

    def test_failure_reason_and_exit_status(self):
        compare_failure(subprocess.CompletedProcess([], 1, '', 'deleted'), 'deleted')
        for code, error in ((0, 'deleted'), (1, 'unrelated'), (127, 'deleted')):
            with self.assertRaises(AssertionError):
                compare_failure(subprocess.CompletedProcess([], code, '', error), 'deleted')

    def test_skip_is_not_success(self):
        result = run([sys.executable, str(Path(__file__).with_name('check_program.py')),
                      '--skip', 'fixture'])
        self.assertEqual(result.returncode, 77)
        self.assertIn('SKIP', result.stdout)

    def test_timeout(self):
        with self.assertRaises(subprocess.TimeoutExpired):
            run([sys.executable, '-c', 'import time; time.sleep(5)'], timeout=0.1)


if __name__ == '__main__':
    unittest.main()
