"""Independent byte-level oracle; do not import the fixture's frame codec."""
import base64
import hashlib
from pathlib import Path
import selectors
import socket
import subprocess
import sys
import threading
import time

case = sys.argv[1]
binary = Path(sys.argv[2]) / "ws_lengths_client"
count = {"empty": 0, "short125": 125, "extended126": 126, "extended255": 255,
         "extended256": 256, "extended65535": 65535, "extended65536": 65536,
         "fixture_empty": 0, "fixture64": 65536, "split64": 65536}.get(case, 256)
message = bytes(97 + i % 26 for i in range(count))
positive = {"default", "empty", "short125", "extended126", "extended255",
            "extended256", "extended65535", "extended65536", "split_header",
            "split_body", "split64", "early_control", "coalesced", "closing_ping",
            "empty_ping", "limit_ping"}
deadline_cases = {"missing_pong", "ping_progress", "body_progress", "idle_close", "idle_tcp"}


def output(pings=1):
    return (f"echo_bytes={count}\nmatched_pong=yes pings_replied={pings}"
            " pongs_ignored=0\nclose=1000 tcp_eof=yes\n").encode()


def exact(peer, size):
    result = bytearray()
    while len(result) < size:
        block = peer.recv(size - len(result))
        assert block, "client ended early"
        result.extend(block)
    return bytes(result)


def client_frame(peer, opcode):
    first, second = exact(peer, 2)
    assert first == 128 | opcode and second & 128, (first, second)
    marker = second & 127
    size = marker
    if marker >= 126:
        size = int.from_bytes(exact(peer, 2 if marker == 126 else 8), "big")
    assert size <= 65536
    assert marker == (size if size <= 125 else 126 if size <= 65535 else 127)
    assert opcode < 8 or size <= 125
    mask = exact(peer, 4)
    body = exact(peer, size)
    return bytes(c ^ mask[i % 4] for i, c in enumerate(body))


def frame(opcode, payload):
    size = len(payload)
    marker = size if size <= 125 else 126 if size <= 65535 else 127
    extra = b"" if marker < 126 else size.to_bytes(2 if marker == 126 else 8, "big")
    return bytes([128 | opcode, marker]) + extra + payload


if case == "invalid":
    for args in ([], ["0"], ["x"], ["65536"], ["1", "-1"], ["1", "65537"],
                 ["1", "4294967296"], ["1", ""], ["1", "x"], ["1", "1", "0"],
                 ["1", "1", "10001"], ["1", "1", "x"], ["1", "1", "1", "extra"]):
        result = subprocess.run([binary, *args], capture_output=True, timeout=3)
        assert result.returncode == 1 and not result.stdout, result
    sys.exit(0)

if case.startswith("fixture"):
    fixture = Path(__file__).resolve().parents[1] / "fixtures/ws_lengths.py"
    process = subprocess.Popen([sys.executable, str(fixture), "0"],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(process.stdout, selectors.EVENT_READ)
            assert ready.select(3), "fixture did not announce its port"
        line = process.stdout.readline().split()
        assert line[0] == "LISTEN", line
        result = subprocess.run([binary, line[1], str(count)], capture_output=True, timeout=5)
        assert result.returncode == 0 and result.stdout == output(), result
        stdout, stderr = process.communicate(timeout=3)
        assert process.returncode == 0 and f"echo_bytes={count} close=1000" in stdout, stderr
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
    result = subprocess.run([binary, str(port)], capture_output=True, timeout=4)
    assert result.returncode == 1 and b"connect" in result.stderr, result
    sys.exit(0)
stop = threading.Event()
failures = []


def peer():
    try:
        stream, _ = listener.accept()
        with stream:
            stream.settimeout(3)
            request = bytearray()
            while not request.endswith(b"\r\n\r\n"):
                request.extend(exact(stream, 1))
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
            stream.sendall(upgrade + (ping if case == "early_control" else b""))
            token = client_frame(stream, 9)
            assert len(token) == 8
            assert client_frame(stream, 1) == message
            malformed = {
                "nonminimal16": b"\x81\x7e\x00\x7d",
                "nonminimal64": b"\x81\x7f" + (65535).to_bytes(8, "big"),
                "high_bit": b"\x81\x7f" + (1 << 63).to_bytes(8, "big"),
                "huge": b"\x81\x7f" + ((1 << 63) - 1).to_bytes(8, "big"),
                "over_limit": b"\x81\x7f" + (65537).to_bytes(8, "big"),
                "truncated16": b"\x81\x7e\x01",
                "truncated64": b"\x81\x7f\x00\x00\x00",
                "truncated_body": b"\x81\x7e\x01\x00abc",
                "long_ping16": b"\x89\x7e",
                "long_ping64": b"\x89\x7f",
                "long_pong": b"\x8a\x7e",
                "long_close": b"\x88\x7f",
                "masked": b"\x81\x80",
                "fragmented": b"\x01\x00",
                "rsv": b"\xc1\x00",
                "binary": b"\x82\x00",
                "wrong_echo": frame(1, b"wrong"),
            }
            if case in malformed:
                stream.sendall(malformed[case])
                if case.startswith("truncated"):
                    stream.shutdown(socket.SHUT_WR)
                # Invalid advertised sizes must be rejected without a body/EOF.
                assert stream.recv(1) == b"", "client did not reject malformed input"
                return
            if case == "budget_exceeded":
                stream.sendall(frame(10, b"noise") * 33)
                assert stream.recv(1) == b""
                return
            if case == "ping_progress":
                for _ in range(5):
                    stream.sendall(ping)
                    assert client_frame(stream, 10) == ping_body
                    if stop.wait(0.07):
                        return
                stop.wait(2)
                return
            if case == "body_progress":
                stream.sendall(b"\x81\x7e\x01\x00")
                for _ in range(5):
                    stream.sendall(b"a")
                    if stop.wait(0.07):
                        return
                stop.wait(2)
                return
            if case == "coalesced":
                stream.sendall(ping + frame(1, message) + frame(10, token))
                assert client_frame(stream, 10) == ping_body
                assert client_frame(stream, 8) == b"\x03\xe8"
                stream.sendall(frame(8, b"\x03\xe8"))
                return
            if case != "early_control":
                stream.sendall(ping)
            assert client_frame(stream, 10) == ping_body
            echo = frame(1, message)
            if case in {"split_header", "split64"}:
                header_size = 10 if case == "split64" else 4
                for byte in echo[:header_size]:
                    stream.sendall(bytes([byte]))
                    if stop.wait(0.005):
                        return
                stream.sendall(echo[header_size:])
            elif case == "split_body":
                for offset in range(0, len(echo), 17):
                    stream.sendall(echo[offset:offset + 17])
            else:
                stream.sendall(echo)
            if case == "missing_pong":
                stop.wait(2)
                return
            stream.sendall(frame(10, token) + (ping if case == "closing_ping" else b""))
            assert client_frame(stream, 8) == b"\x03\xe8"
            if case == "closing_ping":
                assert client_frame(stream, 10) == b"\x00check\xff"
            if case == "idle_close":
                stop.wait(2)
                return
            stream.sendall(frame(8, b"\x03\xe8") + (b"x" if case == "trailing" else b""))
            if case == "idle_tcp":
                stop.wait(2)
    except BaseException as error:
        failures.append(error)
    finally:
        listener.close()


worker = threading.Thread(target=peer)
worker.start()
try:
    started = time.monotonic()
    args = [binary, str(port)]
    if case != "default":
        args.append(str(count))
    if case in deadline_cases:
        args.append("600")
    result = subprocess.run(args, capture_output=True, timeout=5)
    elapsed = time.monotonic() - started
    if case in positive:
        assert result.returncode == 0 and result.stdout == output(2 if case == "closing_ping" else 1), result
    else:
        assert result.returncode == 1 and not result.stdout, result
        if case in deadline_cases:
            assert b"deadline" in result.stderr and 0.45 <= elapsed < 2.5, (elapsed, result)
        if case in {"over_limit", "huge"}:
            assert b"65536-byte policy" in result.stderr, result
        if case == "high_bit":
            assert b"high bit" in result.stderr, result
        if case.startswith("nonminimal"):
            assert b"nonminimal" in result.stderr, result
        if case.startswith("long_"):
            assert b"control payload" in result.stderr, result
finally:
    stop.set()
    worker.join(timeout=4)
    assert not worker.is_alive(), "peer thread did not stop"
if failures:
    raise failures[0]
