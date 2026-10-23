"""Check the capability probe without opening a socket or using a public peer."""
from pathlib import Path
import subprocess
import sys


def check(case, binaries):
    arguments = {"ready": [], "argument": ["--help"],
                 "extra_arguments": ["1", "2"]}[case]
    result = subprocess.run([str(binaries / "quic_tools"), *arguments],
                            capture_output=True, text=True, timeout=5)
    if arguments:
        assert result.returncode == 2, result
        assert result.stdout == "", result
        assert result.stderr == "usage: quic_tools\n", result
    else:
        assert result.returncode == 0, result
        lines = result.stdout.splitlines()
        assert len(lines) == 6, lines
        assert lines[0].startswith("headers: OpenSSL "), lines
        assert lines[1].startswith("runtime: OpenSSL "), lines
        assert lines[2:] == ["QUIC client object: ready",
                             "QUIC listener object: ready",
                             "network traffic: none",
                             "HTTP/3: not tested by this probe"], lines
        assert not result.stderr, result


if __name__ == "__main__":
    check(sys.argv[1], Path(sys.argv[2]).resolve())
