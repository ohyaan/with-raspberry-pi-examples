import subprocess
import sys
import unittest

from verify import run


class VerifierTests(unittest.TestCase):
    def test_exact_output(self):
        run([sys.executable, '-c', 'print("fixture")'], expected='fixture\n')
        with self.assertRaises(AssertionError):
            run([sys.executable, '-c', 'print("different")'], expected='fixture\n')

    def test_expected_rejection_is_not_any_failure(self):
        run([sys.executable, '-c', 'print("E0382"); raise SystemExit(1)'], failure='E0382')
        for code in ('print("E0382")', 'raise SystemExit(1)',
                     'print("E0382"); raise SystemExit(2)'):
            with self.assertRaises(RuntimeError):
                run([sys.executable, '-c', code], failure='E0382')

    def test_timeout(self):
        with self.assertRaises(subprocess.TimeoutExpired):
            run([sys.executable, '-c', 'import time; time.sleep(5)'], timeout=0.1)


if __name__ == '__main__':
    unittest.main()
