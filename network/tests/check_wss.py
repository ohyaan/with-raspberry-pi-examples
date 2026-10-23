"""Independent Python TLS peer and byte oracle for bounded WSS."""
import base64
import hashlib
from pathlib import Path
import selectors
import socket
import ssl
import tempfile
import subprocess
import sys
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "fixtures"))
import tls_echo

case = sys.argv[1]
binary = Path(sys.argv[2]) / "wss_client"
size = {"empty": 0, "one": 1, "two_bytes": 2, "limit": 65536,
        "fixture_empty": 0, "fixture_limit": 65536, "single64": 65536,
        "aggregate_limit": 65536, "many": 64, "split64": 65536}.get(case, 256)
message = bytes(97 + i % 26 for i in range(size))
positive = {"normal", "empty", "one", "two_bytes", "limit", "single", "single64",
            "two", "many", "empty_middle", "empty_first", "empty_last",
            "split", "split64", "coalesced", "early_control", "pong_between",
            "closing_ping", "aggregate_limit", "empty_ping", "limit_ping"}
deadline_cases = {"missing_final", "empty_progress", "body_progress",
                  "missing_pong", "idle_close", "shutdown_idle", "handshake_idle",
                  "upgrade_idle", "upgrade_trickle"}
data_count = 1 if case in {"single", "single64"} else 2 if case in {"two", "aggregate_limit"} else 64 if case == "many" else 3


def output():
    pings = 2 if case == "closing_ping" else 1
    ignored = 1 if case == "pong_between" else 0
    frames = 3 if case.startswith("fixture") else data_count
    return (f"verified=localhost protocol=TLSv1.3 alpn=http/1.1\n"
            f"echo_bytes={size}\nsent_frames=3 received_frames={frames}\n"
            f"matched_pong=yes pings_replied={pings} pongs_ignored={ignored}\n"
            "close=1000 tls_close_notify=yes\n").encode()


def exact(peer, count):
    data = bytearray()
    while len(data) < count:
        part = peer.recv(count - len(data))
        assert part, "client ended early"
        data.extend(part)
    return bytes(data)


def read_client(peer, opcode, final=True):
    first, second = exact(peer, 2)
    assert first == (128 if final else 0) | opcode and second & 128, (first, second)
    marker = second & 127
    count = marker if marker < 126 else int.from_bytes(exact(peer, 2 if marker == 126 else 8), "big")
    assert count <= 65536 and marker == (count if count <= 125 else 126 if count <= 65535 else 127)
    assert opcode < 8 or (final and count <= 125)
    mask = exact(peer, 4)
    body = exact(peer, count)
    return bytes(c ^ mask[i % 4] for i, c in enumerate(body))


def frame(opcode, payload, final=True):
    count = len(payload)
    marker = count if count < 126 else 126 if count < 65536 else 127
    extra = b"" if marker < 126 else count.to_bytes(2 if marker == 126 else 8, "big")
    return bytes([(128 if final else 0) | opcode, marker]) + extra + payload


with tempfile.TemporaryDirectory(prefix="network-wss-test-") as temporary:
    directory = Path(temporary) / "certs"
    tls_echo.certificates(directory)
    ca = directory / "ca.pem"
    if case == "invalid":
        for args in ([], ["1"], ["0", str(ca)], ["65536", str(ca)], ["x", str(ca)],
                     ["1", str(ca), "-1"], ["1", str(ca), "65537"],
                     ["1", str(ca), ""], ["1", str(ca), "x"],
                     ["1", str(ca), "4294967296"], ["1", str(ca), "1", "0"],
                     ["1", str(ca), "1", "10001"], ["1", str(ca), "1", "x"],
                     ["1", str(ca), "1", "1", "extra"]):
            result = subprocess.run([binary, *args], capture_output=True, timeout=3)
            assert result.returncode == 1 and not result.stdout, result
        sys.exit(0)
    if case.startswith("fixture"):
        fixture = Path(__file__).resolve().parents[1] / "fixtures/wss.py"
        process = subprocess.Popen([sys.executable, str(fixture), "0", str(directory)],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            with selectors.DefaultSelector() as ready:
                ready.register(process.stdout, selectors.EVENT_READ)
                assert ready.select(3)
            line = process.stdout.readline().split()
            assert line and line[0] == "LISTEN", line
            result = subprocess.run([binary, line[1], str(ca), str(size)], capture_output=True, timeout=5)
            assert result.returncode == 0 and result.stdout == output(), result
            stdout, stderr = process.communicate(timeout=3)
            assert process.returncode == 0 and f"echo_bytes={size} close=1000" in stdout, stderr
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
    rejected = {"wrong_ca", "wrong_host", "no_alpn", "h2_only", "tls12",
                "missing_ca", "bad_ca", "plaintext", "handshake_idle"}
    if case == "wrong_ca":
        other = Path(temporary) / "other"
        tls_echo.certificates(other)
        ca = other / "ca.pem"
    if case == "missing_ca":
        ca = directory / "missing.pem"
    if case == "bad_ca":
        ca = directory / "bad.pem"
        ca.write_text("not a certificate\n")
    if case == "wrong_host":
        extension = directory / "wrong.ext"
        extension.write_text("basicConstraints=critical,CA:FALSE\n"
                             "keyUsage=critical,digitalSignature\n"
                             "extendedKeyUsage=serverAuth\n"
                             "subjectAltName=DNS:wrong.example\n")
        subprocess.run(["openssl", "x509", "-req", "-in", str(directory / "server.csr"),
                        "-CA", str(directory / "ca.pem"), "-CAkey", str(directory / "ca.key"),
                        "-set_serial", "2", "-out", str(directory / "server.pem"),
                        "-days", "2", "-extfile", str(extension)],
                       capture_output=True, check=True, timeout=5)
    context = tls_echo.context(directory)
    names = []
    context.set_servername_callback(lambda _stream, name, _context: names.append(name))
    if case != "no_alpn":
        context.set_alpn_protocols(["h2"] if case == "h2_only" else ["http/1.1"])
    if case == "tls12":
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.maximum_version = ssl.TLSVersion.TLSv1_2
    application_bytes = bytearray()
    stop = threading.Event()
    failures = []


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
                    if case not in rejected:
                        assert stream.selected_alpn_protocol() == "http/1.1"
                        assert names == ["localhost"]

                    request = bytearray()
                    while not request.endswith(b"\r\n\r\n"):
                        part = stream.recv(1)
                        if not part and case in rejected:
                            return
                        assert part, "request ended early"
                        request.extend(part)
                        application_bytes.extend(part)
                        assert len(request) <= 4096
                    lines = request.decode("ascii").split("\r\n")
                    assert lines[0] == "GET /ws HTTP/1.1"
                    fields = {}
                    for line in lines[1:-2]:
                        name, value = line.split(":", 1)
                        assert name.lower() not in fields
                        fields[name.lower()] = value.strip()
                    key = fields.pop("sec-websocket-key")
                    assert len(base64.b64decode(key, validate=True)) == 16
                    assert fields == {"host": f"localhost:{port}", "upgrade": "websocket",
                                      "connection": "Upgrade", "sec-websocket-version": "13"}
                    accept = base64.b64encode(hashlib.sha1(
                        (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
                    upgrade = ("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                               f"Connection: Upgrade\r\nSec-WebSocket-Accept: {accept}\r\n\r\n").encode()
                    ping_body = b"" if case == "empty_ping" else b"x" * 125 if case == "limit_ping" else b"\x00check\xff"
                    ping = frame(9, ping_body)
                    if case == "upgrade_idle":
                        stop.wait(2)
                        return
                    if case == "upgrade_trickle":
                        for byte in upgrade:
                            stream.sendall(bytes([byte]))
                            if stop.wait(0.10):
                                return
                        return
                    if case == "wrong_accept":
                        upgrade = upgrade.replace(accept.encode(), b"X" * 28)
                    if case == "bad_upgrade":
                        upgrade = upgrade.replace(b"101 Switching Protocols", b"200 OK")
                    if case == "extension":
                        upgrade = upgrade.replace(b"\r\n\r\n", b"\r\nSec-WebSocket-Extensions: permessage-deflate\r\n\r\n")
                    if case == "truncated_upgrade":
                        stream.sendall(upgrade[:35])
                        stream.unwrap().close()
                        return
                    stream.sendall(upgrade + (ping if case == "early_control" else b""))
                    if case in {"wrong_accept", "bad_upgrade", "extension"}:
                        assert stream.recv(1) == b""
                        return
                    token = read_client(stream, 9)
                    assert len(token) == 8
                    third = size // 3
                    assert read_client(stream, 1, False) == message[:third]
                    assert read_client(stream, 0, False) == message[third:third * 2]
                    assert read_client(stream, 0, True) == message[third * 2:]
                    if case == "close_notify_mid":
                        stream.sendall(frame(1, message[:10], False))
                        stream.unwrap().close()
                        return
                    malformed = {
                        "orphan": frame(0, b"x"), "orphan_more": frame(0, b"", False),
                        "new_text": frame(1, b"a", False) + frame(1, b"b"),
                        "new_text_more": frame(1, b"a", False) + frame(1, b"b", False),
                        "binary": frame(2, b"x"),
                        "binary_during": frame(1, b"a", False) + frame(2, b"b"),
                        "fragmented_ping": frame(9, b"x", False),
                        "fragmented_pong": frame(10, b"", False),
                        "fragmented_close": frame(8, b"\x03\xe8", False),
                        "rsv": b"\x41\x00", "masked": b"\x01\x80",
                        "reserved": frame(3, b""),
                        "long_control": b"\x89\x7e",
                        "nonminimal": b"\x01\x7e\x00\x7d",
                        "high_bit": b"\x01\x7f" + (1 << 63).to_bytes(8, "big"),
                        "frame_over": b"\x01\x7f" + (65537).to_bytes(8, "big"),
                        "aggregate_over": frame(1, b"a" * 65536, False) + frame(0, b"x"),
                        "too_many": frame(1, b"", False) + frame(0, b"", False) * 64,
                        "early_close": frame(8, b"\x03\xe8"),
                        "close_during": frame(1, b"a", False) + frame(8, b"\x03\xe8"),
                        "wrong_echo": frame(1, b"x" * size),
                        "wrong_order": frame(1, message[size // 2:], False) + frame(0, message[:size // 2]),
                        "duplicate": frame(1, message) + frame(1, message),
                        "truncated_header": b"\x01\x7e\x01",
                        "truncated_body": b"\x01\x7e\x01\x00abc",
                        "eof_mid": frame(1, message, False),
                    }
                    if case in malformed:
                        stream.sendall(malformed[case])
                        if case in {"truncated_header", "truncated_body", "eof_mid"}:
                            stream.unwrap().close()
                        assert stream.recv(1) == b"", "client accepted invalid/incomplete message"
                        return
                    if case == "control_budget":
                        stream.sendall(frame(10, b"noise") * 33)
                        assert stream.recv(1) == b""
                        return
                    if case == "body_progress":
                        stream.sendall(b"\x01\x7e\x01\x00")
                        for _ in range(5):
                            stream.sendall(b"a")
                            if stop.wait(0.07):
                                return
                        stop.wait(2)
                        return
                    if case in {"empty_progress", "missing_final"}:
                        stream.sendall(frame(1, message, False))
                        if case == "empty_progress":
                            for _ in range(5):
                                stream.sendall(frame(0, b"", False))
                                if stop.wait(0.07):
                                    return
                        stop.wait(2)
                        return
                    half = size // 2
                    parts = [frame(1, message[:half], False), frame(0, b"", False),
                             frame(0, message[half:])]
                    if case in {"single", "single64"}:
                        parts = [frame(1, message)]
                    elif case in {"two", "aggregate_limit"}:
                        parts = [frame(1, message[:half], False), frame(0, message[half:])]
                    elif case == "many":
                        parts = [frame(1 if i == 0 else 0, message[i:i + 1], i == 63) for i in range(64)]
                    elif case == "empty_first":
                        parts = [frame(1, b"", False), frame(0, message[:half], False), frame(0, message[half:])]
                    elif case == "empty_last":
                        parts = [frame(1, message[:half], False), frame(0, message[half:], False), frame(0, b"")]
                    elif case == "split64":
                        parts = [frame(1, message, False), frame(0, b"", False), frame(0, b"")]
                    if case == "coalesced":
                        stream.sendall(parts[0] + ping + frame(10, token) + b"".join(parts[1:]))
                        assert read_client(stream, 10) == ping_body
                    else:
                        if case == "split64":
                            for byte in parts[0][:10]:
                                stream.sendall(bytes([byte]))
                                if stop.wait(0.005):
                                    return
                            stream.sendall(parts[0][10:])
                        elif case == "split":
                            for part in (parts[0][:1], parts[0][1:3], parts[0][3:]):
                                stream.sendall(part)
                                if stop.wait(0.005):
                                    return
                        else:
                            stream.sendall(parts[0])
                        if case != "early_control":
                            stream.sendall(ping)
                        # Require Pong before sending the final continuation.
                        assert read_client(stream, 10) == ping_body
                        if case == "pong_between":
                            stream.sendall(frame(10, b"notice"))
                        if case != "missing_pong":
                            stream.sendall(frame(10, token))
                        for part in parts[1:]:
                            stream.sendall(part)
                    if case == "missing_pong":
                        stop.wait(2)
                        return
                    if case == "closing_ping":
                        stream.sendall(ping)
                    assert read_client(stream, 8) == b"\x03\xe8"
                    if case == "closing_ping":
                        assert read_client(stream, 10) == ping_body
                    if case == "data_during_close":
                        stream.sendall(frame(0, b""))
                        assert stream.recv(1) == b""
                        return
                    if case == "no_ws_close":
                        stream.unwrap().close()
                        return
                    if case == "idle_close":
                        stop.wait(2)
                        return
                    stream.sendall(frame(8, b"\x03\xe8") + (b"x" if case == "trailing" else b""))
                    if case == "abrupt":
                        return  # Complete WS Close, but no TLS close_notify.
                    if case == "shutdown_idle":
                        stop.wait(2)
                        return
                    stream.unwrap().close()
        except (ssl.SSLError, OSError) as error:
            if case in positive:
                failures.append(error)
        except BaseException as error:
            failures.append(error)
        finally:
            listener.close()


    worker = threading.Thread(target=peer)
    if case not in {"missing_ca", "bad_ca"}:
        worker.start()
    try:
        started = time.monotonic()
        result = subprocess.run([binary, str(port), str(ca), str(size),
                                 "600" if case in deadline_cases else "3000"],
                                capture_output=True, timeout=5)
        elapsed = time.monotonic() - started
        if case in positive:
            assert result.returncode == 0 and result.stdout == output(), result
        else:
            assert result.returncode == 1 and not result.stdout, result
            if case in deadline_cases:
                assert b"deadline" in result.stderr and 0.45 <= elapsed < 2.5, (elapsed, result)
            expected_errors = {"orphan": b"continuation state", "orphan_more": b"continuation state",
                               "new_text": b"continuation state", "new_text_more": b"continuation state",
                               "aggregate_over": b"message exceeds", "too_many": b"data-frame budget",
                               "frame_over": b"payload exceeds", "control_budget": b"Ping/Pong frame budget"}
            if case in expected_errors:
                assert expected_errors[case] in result.stderr, result
    finally:
        stop.set()
        if worker.ident is not None:
            worker.join(timeout=4)
        assert not worker.is_alive(), "peer thread did not stop"
        listener.close()
    if failures:
        raise failures[0]
    if case in rejected:
        assert not application_bytes, application_bytes
    if case == "abrupt":
        assert b"clean close_notify" in result.stderr, result
    if case in {"wrong_ca", "wrong_host", "tls12", "plaintext"}:
        assert b"TLS operation failed" in result.stderr, result
    if case in {"no_alpn", "h2_only"}:
        assert b"server did not select HTTP/1.1" in result.stderr, result
    if case in {"missing_ca", "bad_ca"}:
        assert b"load CA file" in result.stderr, result
