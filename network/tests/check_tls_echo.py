"""Bounded independent Python TLS peer for the standalone C++ client."""
from pathlib import Path
import socket
import ssl
import selectors
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "fixtures"))
import tls_echo

case = sys.argv[1]
binary = Path(sys.argv[2]) / "tls_echo_client"
message = b"Hello over TLS.\n"
with tempfile.TemporaryDirectory(prefix="network-tls-test-") as temporary:
    directory = Path(temporary) / "certs"
    tls_echo.certificates(directory)
    ca = directory / "ca.pem"
    if case == "fixture":
        for hostname, succeeds in (("localhost", True), ("wrong.example", False)):
            verified = subprocess.run([
                "openssl", "verify", "-CAfile", str(ca), "-purpose", "sslserver",
                "-verify_hostname", hostname, str(directory / "server.pem"),
            ], capture_output=True, timeout=3)
            assert (verified.returncode == 0) == succeeds, verified
        process = subprocess.Popen([
            sys.executable, str(Path(tls_echo.__file__)), "serve", "0", str(directory),
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            with selectors.DefaultSelector() as ready:
                ready.register(process.stdout, selectors.EVENT_READ)
                assert ready.select(3), "fixture did not announce its port"
            announcement = process.stdout.readline().split()
            assert announcement[0] == "LISTEN", announcement
            result = subprocess.run([binary, announcement[1], str(ca)],
                                    capture_output=True, timeout=5)
            assert result.returncode == 0, result
            assert result.stdout == b"verified=localhost protocol=TLSv1.3\n" + message
            _, error = process.communicate(timeout=3)
            assert process.returncode == 0, error
        finally:
            if process.poll() is None:
                process.terminate()
                process.communicate(timeout=3)
        sys.exit(0)
    if case == "invalid":
        for args in ([], ["0", str(ca)], ["65536", str(ca)],
                     ["12x", str(ca)], ["1", str(ca), ""],
                     ["1", str(ca), "local host"],
                     ["1", str(ca), "localhost", "0"],
                     ["1", str(ca), "localhost", "10001"],
                     ["1", str(ca), "localhost", "x"]):
            result = subprocess.run([binary, *args], capture_output=True, timeout=3)
            assert result.returncode == 1 and not result.stdout, result
        sys.exit(0)
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(3)
    port = listener.getsockname()[1]
    if case == "refused":
        listener.close()
        result = subprocess.run([binary, str(port), str(ca)],
                                capture_output=True, timeout=4)
        assert result.returncode == 1 and b"connect" in result.stderr, result
        sys.exit(0)
    if case == "wrong_ca":
        other = Path(temporary) / "other"
        tls_echo.certificates(other)
        ca = other / "ca.pem"
    if case == "missing_ca":
        ca = directory / "missing.pem"
    if case == "bad_ca":
        ca = directory / "bad.pem"
        ca.write_text("not a certificate\n")
    failures = []
    requests = []
    application_bytes = bytearray()
    tls_context = tls_echo.context(directory)
    if case == "tls12":
        tls_context.minimum_version = ssl.TLSVersion.TLSv1_2
        tls_context.maximum_version = ssl.TLSVersion.TLSv1_2
    stop = threading.Event()

    def peer():
        try:
            raw, _ = listener.accept()
            with raw:
                raw.settimeout(2)
                if case == "handshake_idle":
                    stop.wait(2)
                    return
                if case == "plaintext":
                    raw.recv(4096)
                    raw.sendall(b"HTTP/1.1 200 OK\r\n\r\n")
                    return
                with tls_context.wrap_socket(raw, server_side=True) as stream:
                    request = bytearray()
                    while len(request) < len(message):
                        part = stream.recv(len(message) - len(request))
                        if not part:
                            raise AssertionError("request ended early")
                        request.extend(part)
                        application_bytes.extend(part)
                    requests.append(bytes(request))
                    assert bytes(request) == message, request
                    if case == "idle":
                        stop.wait(2)
                        return
                    if case == "abrupt":
                        stream.sendall(message)
                        return  # No TLS close_notify.
                    if case == "short":
                        stream.sendall(message[:3])
                    elif case == "wrong_echo":
                        stream.sendall(b"X" * len(message))
                    elif case in ("split", "trickle"):
                        for byte in message:
                            stream.sendall(bytes([byte]))
                            if stop.wait(0.10 if case == "trickle" else 0.005):
                                return
                    else:
                        stream.sendall(message)
                    if case == "shutdown_idle":
                        stop.wait(2)
                        return
                    stream.unwrap().close()
        except (ssl.SSLError, ConnectionError, OSError) as error:
            # Negative cases intentionally break the TLS exchange.
            if case in ("good", "split"):
                failures.append(error)
        except BaseException as error:
            failures.append(error)

    worker = threading.Thread(target=peer)
    # Missing/bad trust files fail before TCP setup; no accept thread is needed.
    if case not in ("missing_ca", "bad_ca"):
        worker.start()
    started = time.monotonic()
    try:
        result = subprocess.run([
            binary, str(port), str(ca),
            "wrong.example" if case == "wrong_host" else "localhost", "600",
        ], capture_output=True, timeout=4)
        elapsed = time.monotonic() - started
    finally:
        stop.set()
        if worker.ident is not None:
            worker.join(3)
            assert not worker.is_alive(), "fixture did not finish"
        listener.close()
    assert not failures, failures
    if case in ("good", "split"):
        assert result.returncode == 0, result
        assert result.stdout == b"verified=localhost protocol=TLSv1.3\n" + message, result
        assert requests == [message], requests
    else:
        assert result.returncode == 1 and not result.stdout, result
    if case in ("handshake_idle", "idle", "trickle", "shutdown_idle"):
        assert b"session deadline expired" in result.stderr, result
        assert 0.45 < elapsed < 2.5, elapsed
    if case in ("wrong_ca", "wrong_host", "tls12"):
        assert not application_bytes and b"TLS operation failed" in result.stderr, result
    if case == "wrong_echo":
        assert b"echo mismatch" in result.stderr, result
