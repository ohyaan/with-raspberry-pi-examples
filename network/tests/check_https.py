"""Independent bounded TLS peer checks HTTP framing inside the secure stream."""
from pathlib import Path
import selectors
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "fixtures"))
import tls_echo

case = sys.argv[1]
binary = Path(sys.argv[2]) / "https_client"
positive = {"combined", "split", "binary", "empty", "mixed_case", "body_limit", "header_limit"}
rejected = {"wrong_ca", "wrong_host", "tls12", "no_alpn", "h2_only"}
deadline_cases = {"handshake_idle", "idle", "trickle", "shutdown_idle"}

with tempfile.TemporaryDirectory(prefix="network-https-test-") as temporary:
    directory = Path(temporary) / "certs"
    tls_echo.certificates(directory)
    ca = directory / "ca.pem"
    if case == "invalid":
        for args in ([], ["0", str(ca)], ["65536", str(ca)], ["x", str(ca)],
                     ["1", str(ca), ""], ["1", str(ca), "http://localhost/"],
                     ["1", str(ca), "/with space"], ["1", str(ca), "/#fragment"],
                     ["1", str(ca), "/\r\nInjected: x"],
                     ["1", str(ca), "/" + "x" * 200],
                     ["1", str(ca), "/", "0"], ["1", str(ca), "/", "10001"],
                     ["1", str(ca), "/", "12x"]):
            result = subprocess.run([binary, *args], capture_output=True, timeout=3)
            assert result.returncode == 1 and not result.stdout, result
        sys.exit(0)
    if case in ("fixture", "fixture_missing"):
        fixture = Path(tls_echo.__file__).with_name("https_server.py")
        process = subprocess.Popen([sys.executable, str(fixture), "0", str(directory)],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            with selectors.DefaultSelector() as ready:
                ready.register(process.stdout, selectors.EVENT_READ)
                assert ready.select(3), "fixture did not announce its port"
            announcement = process.stdout.readline().split()
            assert announcement[0] == "LISTEN", announcement
            path = "/missing" if case == "fixture_missing" else "/hello.txt"
            result = subprocess.run([binary, announcement[1], str(ca), path],
                                    capture_output=True, timeout=5)
            body = b"Not found\n" if case == "fixture_missing" else b"Hello over HTTPS.\n"
            status = 404 if case == "fixture_missing" else 200
            expected = (b"verified=localhost protocol=TLSv1.3 alpn=http/1.1\n"
                        + f"status={status} bytes={len(body)}\n".encode() + body)
            assert result.returncode == 0 and result.stdout == expected, result
            _, error = process.communicate(timeout=3)
            assert process.returncode == 0, error
        finally:
            if process.poll() is None:
                process.terminate()
                process.communicate(timeout=3)
        sys.exit(0)

    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(3)
    port = listener.getsockname()[1]
    if case == "refused":
        listener.close()
        result = subprocess.run([binary, str(port), str(ca)], capture_output=True, timeout=4)
        assert result.returncode == 1 and b"connect" in result.stderr, result
        sys.exit(0)
    if case == "wrong_ca":
        other = Path(temporary) / "other"
        tls_echo.certificates(other)
        ca = other / "ca.pem"
    if case == "wrong_host":
        extension = directory / "wrong.ext"
        extension.write_text((directory / "server.ext").read_text().replace(
            "DNS:localhost", "DNS:wrong.example"), encoding="ascii")
        subprocess.run([
            "openssl", "x509", "-req", "-in", "server.csr", "-CA", "ca.pem",
            "-CAkey", "ca.key", "-CAserial", "ca.srl", "-out", "server.pem",
            "-days", "2", "-extfile", "wrong.ext",
        ], cwd=directory, check=True, capture_output=True, timeout=5)
    if case == "missing_ca":
        ca = directory / "missing.pem"
    if case == "bad_ca":
        ca = directory / "bad.pem"
        ca.write_text("not a certificate\n")
    context = tls_echo.context(directory)
    if case != "no_alpn":
        context.set_alpn_protocols(["h2" if case == "h2_only" else "http/1.1"])
    if case == "tls12":
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.maximum_version = ssl.TLSVersion.TLSv1_2
    names = []
    context.set_servername_callback(lambda stream, name, ctx: names.append(name))
    application_bytes = bytearray()
    failures = []
    stop = threading.Event()
    body = b"hello\n"
    if case == "binary":
        body = b"\x00\xff\r\n"
    elif case in ("empty", "header_limit"):
        body = b""
    elif case == "body_limit":
        body = b"x" * 4096
    headers = b"HTTP/1.1 200 OK\r\nContent-Length: " + str(len(body)).encode() + b"\r\n\r\n"
    responses = {
        "missing_length": b"HTTP/1.1 200 OK\r\n\r\nhello\n",
        "duplicate_length": b"HTTP/1.1 200 OK\r\nContent-Length: 6\r\nContent-Length: 6\r\n\r\nhello\n",
        "conflicting_length": b"HTTP/1.1 200 OK\r\nContent-Length: 6\r\nContent-Length: 7\r\n\r\nhello\n",
        "negative_length": b"HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\n",
        "overflow_length": b"HTTP/1.1 200 OK\r\nContent-Length: 999999999999999999999999\r\n\r\n",
        "oversized_length": b"HTTP/1.1 200 OK\r\nContent-Length: 4097\r\n\r\n",
        "transfer_encoding": b"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 6\r\n\r\nhello\n",
        "content_encoding": b"HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 6\r\n\r\nhello\n",
        "malformed_header": b"HTTP/1.1 200 OK\r\nBad Field: x\r\nContent-Length: 6\r\n\r\nhello\n",
        "invalid_value": b"HTTP/1.1 200 OK\r\nX: \x00\r\nContent-Length: 6\r\n\r\nhello\n",
        "folded_header": b"HTTP/1.1 200 OK\r\nX: a\r\n b\r\nContent-Length: 6\r\n\r\nhello\n",
        "bad_status": b"HTTP/1.1 500 Error\r\nContent-Length: 0\r\n\r\n",
        "http10": b"HTTP/1.0 200 OK\r\nContent-Length: 0\r\n\r\n",
        "truncated_headers": b"HTTP/1.1 200 OK\r\nContent-Len",
        "truncated_body": b"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nabc",
    }
    response = responses.get(case, headers + body)
    if case == "mixed_case":
        response = response.replace(b"Content-Length: ", b"cOnTeNt-LeNgTh:\t ")
    if case in ("header_limit", "oversized_headers"):
        prefix = b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\nX-Pad: "
        size = 4096 if case == "header_limit" else 4097
        response = prefix + b"x" * (size - len(prefix) - 4) + b"\r\n\r\n"
        assert len(response) == size
    if case == "surplus":
        response += b"unexpected"

    def peer():
        try:
            raw, _ = listener.accept()
            with raw:
                raw.settimeout(3)
                if case == "handshake_idle":
                    stop.wait(2)
                    return
                if case == "plaintext":
                    raw.recv(4096)
                    raw.sendall(b"HTTP/1.1 200 OK\r\n\r\n")
                    return
                with context.wrap_socket(raw, server_side=True) as stream:
                    while b"\r\n\r\n" not in application_bytes:
                        part = stream.recv(128)
                        if not part:
                            if case in rejected:
                                return
                            raise AssertionError("request ended early")
                        application_bytes.extend(part)
                        assert len(application_bytes) <= 4096
                    assert bytes(application_bytes) == (
                        f"GET /hello.txt HTTP/1.1\r\nHost: localhost:{port}\r\n"
                        "Connection: close\r\n\r\n").encode(), application_bytes
                    assert stream.selected_alpn_protocol() == "http/1.1"
                    assert names == ["localhost"], names
                    if case == "idle":
                        stop.wait(2)
                        return
                    if case in ("split", "trickle"):
                        for byte in response:
                            stream.sendall(bytes([byte]))
                            if stop.wait(0.10 if case == "trickle" else 0.001):
                                return
                    else:
                        stream.sendall(response)
                    if case == "abrupt":
                        return  # TCP EOF without close_notify, despite a complete HTTP body.
                    if case == "shutdown_idle":
                        stop.wait(2)
                        return
                    if case in positive:
                        # HTTP body complete before the peer closes TLS.
                        stop.wait(0.10)
                    stream.unwrap().close()
        except (ssl.SSLError, OSError) as error:
            if case in positive:
                failures.append(error)
        except BaseException as error:
            failures.append(error)

    worker = threading.Thread(target=peer)
    if case not in ("missing_ca", "bad_ca"):
        worker.start()
    started = time.monotonic()
    try:
        result = subprocess.run([
            binary, str(port), str(ca), "/hello.txt",
            "600" if case in deadline_cases else "3000",
        ], capture_output=True, timeout=5)
        elapsed = time.monotonic() - started
    finally:
        stop.set()
        if worker.ident is not None:
            worker.join(4)
            assert not worker.is_alive(), "fixture did not finish"
        listener.close()
    assert not failures, failures
    if case in positive:
        expected = (b"verified=localhost protocol=TLSv1.3 alpn=http/1.1\n"
                    + f"status=200 bytes={len(body)}\n".encode() + body)
        assert result.returncode == 0 and result.stdout == expected, result
    else:
        assert result.returncode == 1 and not result.stdout, result
    if case in rejected:
        assert not application_bytes, application_bytes
    if case in deadline_cases:
        assert b"session deadline expired" in result.stderr, result
        assert 0.45 < elapsed < 2.5, elapsed
    if case == "surplus":
        assert b"surplus response bytes" in result.stderr, result
    if case == "abrupt":
        assert b"clean close_notify" in result.stderr, result
