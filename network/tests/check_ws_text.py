"""Independent wire oracle for masking, short frames and normal Close."""
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
binary = Path(sys.argv[2]) / "ws_text_client"
message = b"Hello WebSocket!"
if case in ("empty", "fixture_empty"):
    message = b""
if case in ("limit", "fixture_limit"):
    message = b"x" * 125
positive = {"echo", "empty", "limit", "split", "header_split", "coalesced", "early_echo", "fresh_masks"}
deadline_cases = {"idle_echo", "idle_close", "idle_tcp", "trickle", "payload_trickle"}


def exact(peer, count):
    data = bytearray()
    while len(data) < count:
        part = peer.recv(count - len(data))
        assert part, "client TCP ended early"
        data.extend(part)
    return bytes(data)


def client_frame(peer, expected_opcode, masks):
    first, second = exact(peer, 2)
    assert first == 0x80 | expected_opcode, (first, expected_opcode)
    assert second & 128 and second & 127 <= 125, second
    mask = exact(peer, 4)
    masks.append(mask)
    encoded = exact(peer, second & 127)
    return bytes(value ^ mask[index % 4] for index, value in enumerate(encoded))


def expected_output():
    return f"echo_bytes={len(message)}\n".encode() + message + b"\nclose=1000 tcp_eof=yes\n"


if case == "invalid":
    for args in ([], ["0"], ["65536"], ["x"], ["1", "x" * 126], ["1", "\n"],
                 ["1", "日本語"], ["1", "hello", "0"], ["1", "hello", "10001"],
                 ["1", "hello", "x"], ["1", "hello", "1", "extra"]):
        result = subprocess.run([binary, *args], capture_output=True, timeout=3)
        assert result.returncode == 1 and not result.stdout, result
    sys.exit(0)
if case.startswith("fixture"):
    fixture = Path(__file__).resolve().parents[1] / "fixtures/ws_text.py"
    process = subprocess.Popen([sys.executable, str(fixture), "0"],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(process.stdout, selectors.EVENT_READ)
            assert ready.select(3), "fixture did not announce its port"
        announcement = process.stdout.readline().split()
        assert announcement[0] == "LISTEN", announcement
        result = subprocess.run([binary, announcement[1], message.decode()],
                                capture_output=True, timeout=5)
        assert result.returncode == 0 and result.stdout == expected_output(), result
        output, error = process.communicate(timeout=3)
        assert process.returncode == 0 and "close=1000" in output, error
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
masks = []
keys = []
repetitions = 4 if case == "fresh_masks" else 1


def peer():
    try:
        for _ in range(repetitions):
            stream, _ = listener.accept()
            with stream:
                stream.settimeout(3)
                request = bytearray()
                while b"\r\n\r\n" not in request:
                    request.extend(exact(stream, 1))
                    assert len(request) <= 4096
                lines = request.decode("ascii").split("\r\n")
                assert lines[0] == "GET /ws HTTP/1.1" and lines[-2:] == ["", ""], lines
                fields = {}
                for line in lines[1:-2]:
                    name, value = line.split(":", 1)
                    assert name.lower() not in fields
                    fields[name.lower()] = value.strip()
                key = fields.pop("sec-websocket-key")
                assert len(base64.b64decode(key, validate=True)) == 16
                keys.append(key)
                assert fields == {"host": f"localhost:{port}", "upgrade": "websocket",
                                  "connection": "Upgrade", "sec-websocket-version": "13"}, fields
                # No frame or write-side EOF before the opening response.
                stream.setblocking(False)
                try:
                    extra = stream.recv(1, socket.MSG_PEEK)
                except BlockingIOError:
                    pass
                else:
                    raise AssertionError(f"unexpected pre-upgrade data/EOF: {extra!r}")
                finally:
                    stream.settimeout(3)
                accept = base64.b64encode(hashlib.sha1(
                    (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
                response = ("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                            f"Connection: Upgrade\r\nSec-WebSocket-Accept: {accept}\r\n\r\n").encode()
                if case == "wrong_accept":
                    response = response.replace(accept.encode(), b"X" * 28)
                if case == "bad_upgrade":
                    response = response.replace(b"101 Switching Protocols", b"200 OK")
                if case == "extension":
                    response = response[:-2] + b"Sec-WebSocket-Extensions: permessage-deflate\r\n\r\n"
                echo = bytes([0x81, len(message)]) + message
                if case == "early_echo":
                    stream.sendall(response + echo)
                elif case == "header_split":
                    for byte in response:
                        stream.sendall(bytes([byte]))
                        if stop.wait(0.001):
                            return
                else:
                    stream.sendall(response)
                if case in ("wrong_accept", "bad_upgrade", "extension"):
                    assert stream.recv(1) == b"", "client sent a frame after rejected handshake"
                    return
                assert client_frame(stream, 0x1, masks) == message
                if case == "idle_echo":
                    stop.wait(2)
                    return
                changes = {
                    "binary_opcode": b"\x82\x00", "fragmented": b"\x01\x00",
                    "rsv": b"\xc1\x00", "masked_server": b"\x81\x80",
                    "extended16": b"\x81\x7e", "extended64": b"\x81\x7f",
                    "wrong_echo": b"\x81\x01x", "close_early": b"\x88\x02\x03\xe8",
                    "ping": b"\x89\x00", "pong": b"\x8a\x00",
                    "truncated_header": b"\x81", "truncated_body": b"\x81\x05x",
                }
                echo = changes.get(case, echo)
                close = b"\x88\x02\x03\xe8"
                if case == "coalesced":
                    stream.sendall(echo + close)
                elif case in ("split", "trickle", "payload_trickle"):
                    for index, byte in enumerate(echo):
                        stream.sendall(bytes([byte]))
                        delay = 0.10 if case == "trickle" or (case == "payload_trickle" and index >= 2) else 0.001
                        if stop.wait(delay):
                            return
                elif case != "early_echo":
                    stream.sendall(echo)
                if case in changes:
                    return
                assert client_frame(stream, 0x8, masks) == b"\x03\xe8"
                if case == "idle_close":
                    stop.wait(2)
                    return
                if case == "eof_no_close":
                    return
                close = {
                    "wrong_close_code": b"\x88\x02\x03\xed",
                    "valid_close_code": b"\x88\x02\x03\xe9",
                    "empty_close": b"\x88\x00",
                    "close_one_byte": b"\x88\x01\x03",
                    "close_reason": b"\x88\x03\x03\xe8x",
                    "data_instead_close": b"\x81\x00",
                    "masked_close": b"\x88\x82",
                    "fragmented_close": b"\x08\x00",
                    "rsv_close": b"\xc8\x00",
                    "close_extended": b"\x88\x7e",
                    "truncated_close": b"\x88\x02\x03",
                    "eof_after_close_header": b"\x88\x02",
                }.get(case, close)
                if case == "extra_after_close":
                    close += b"!"
                if case == "split":
                    for byte in close:
                        stream.sendall(bytes([byte]))
                        stop.wait(0.001)
                elif case != "coalesced":
                    stream.sendall(close)
                if case == "idle_tcp":
                    stop.wait(2)
                    return
                # Closing TCP after the Close reply is part of this happy path.
    except OSError as error:
        if case in positive:
            failures.append(error)
    except BaseException as error:
        failures.append(error)


worker = threading.Thread(target=peer)
worker.start()
started = time.monotonic()
try:
    for _ in range(repetitions):
        result = subprocess.run([binary, str(port), message.decode(),
                                 "600" if case in deadline_cases else "3000"],
                                capture_output=True, timeout=5)
        if case in positive:
            assert result.returncode == 0 and result.stdout == expected_output(), result
        else:
            assert result.returncode == 1 and not result.stdout, result
    elapsed = time.monotonic() - started
finally:
    stop.set()
    worker.join(4)
    listener.close()
assert not worker.is_alive() and not failures, failures
if case in deadline_cases:
    assert b"session deadline expired" in result.stderr, result
    assert 0.45 < elapsed < 2.5, elapsed
if case == "fresh_masks":
    assert len(masks) == 8 and len(set(masks)) > 1 and len(set(keys)) == 4, masks
    assert any(masks[index] != masks[index + 1] for index in range(0, 8, 2)), masks
