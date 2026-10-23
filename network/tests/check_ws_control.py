"""Independent wire oracle for Ping/Pong interleaving and correlation."""
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
binary = Path(sys.argv[2]) / "ws_control_client"
message = b"Hello WebSocket!"
if case in ("empty", "fixture_empty"):
    message = b""
if case in ("limit", "fixture_limit"):
    message = b"x" * 125
positive = {"normal", "empty", "limit", "pong_first", "echo_first", "empty_ping",
            "binary_ping", "limit_ping", "split", "coalesced", "early_control",
            "unsolicited", "duplicate_pong", "no_ping", "closing_ping", "closing_pong",
            "budget_limit", "closing_budget_limit"}
deadline_cases = {"wrong_pong", "missing_pong", "pong_flood", "ping_progress", "idle_close", "idle_tcp"}


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
    pings = 0 if case in {"no_ping", "budget_limit", "closing_budget_limit"} else 1
    ignored = 1 if case in {"unsolicited", "coalesced", "duplicate_pong", "closing_pong"} else 0
    if case in {"budget_limit", "closing_budget_limit"}:
        ignored = 31
    if case == "closing_ping":
        pings = 2
    if case.startswith("fixture"):
        pings, ignored = 2, 1
    return (f"echo_bytes={len(message)}\n".encode() + message +
            f"\nmatched_pong=yes pings_replied={pings} pongs_ignored={ignored}\n".encode() +
            b"close=1000 tcp_eof=yes\n")


if case == "invalid":
    for args in ([], ["0"], ["65536"], ["x"], ["1", "x" * 126], ["1", "\n"],
                 ["1", "日本語"], ["1", "hello", "0"], ["1", "hello", "10001"],
                 ["1", "hello", "x"], ["1", "hello", "1", "extra"]):
        result = subprocess.run([binary, *args], capture_output=True, timeout=3)
        assert result.returncode == 1 and not result.stdout, result
    sys.exit(0)
if case.startswith("fixture"):
    fixture = Path(__file__).resolve().parents[1] / "fixtures/ws_control.py"
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
repetitions = 1



def peer():
    try:
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
            assert fields == {"host": f"localhost:{port}", "upgrade": "websocket",
                              "connection": "Upgrade", "sec-websocket-version": "13"}, fields
            accept = base64.b64encode(hashlib.sha1(
                (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
            response = ("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                        f"Connection: Upgrade\r\nSec-WebSocket-Accept: {accept}\r\n\r\n").encode()
            if case == "wrong_accept":
                response = response.replace(accept.encode(), b"X" * 28)
            if case == "bad_upgrade":
                response = response.replace(b"101 Switching Protocols", b"200 OK")

            def frame(opcode, payload):
                assert len(payload) <= 125
                return bytes([0x80 | opcode, len(payload)]) + payload

            def transmit(data, split=False):
                if split:
                    for byte in data:
                        stream.sendall(bytes([byte]))
                        if stop.wait(0.001):
                            return
                else:
                    stream.sendall(data)

            ping = b"check"
            if case == "empty_ping":
                ping = b""
            if case == "binary_ping":
                ping = b"\x00\xff\x80\n"
            if case == "limit_ping":
                ping = bytes(range(125))
            if case == "early_control":
                stream.sendall(response + frame(0x9, ping))
            else:
                stream.sendall(response)
            if case in {"wrong_accept", "bad_upgrade"}:
                assert stream.recv(1) == b"", "control/data sent after rejected handshake"
                return
            token = client_frame(stream, 0x9, masks)
            assert len(token) == 8, token
            assert client_frame(stream, 0x1, masks) == message

            malformed = {
                "fragmented_ping": b"\x09\x00", "rsv_ping": b"\xc9\x00",
                "masked_ping": b"\x89\x80", "long_ping16": b"\x89\x7e",
                "long_ping64": b"\x89\x7f", "fragmented_pong": b"\x0a\x00",
                "rsv_pong": b"\xca\x00", "masked_pong": b"\x8a\x80",
                "long_pong": b"\x8a\x7e", "reserved_opcode": b"\x8b\x00",
                "early_close": b"\x88\x02\x03\xe8", "truncated_ping": b"\x89\x03x",
            }
            if case in malformed:
                stream.sendall(malformed[case])
                return
            if case == "budget_exceeded":
                stream.sendall(frame(0xa, b"noise") * 33)
                return
            if case == "pong_flood":
                stream.sendall(frame(0x1, message))
                for _ in range(20):
                    stream.sendall(frame(0xa, b"noise"))
                    if stop.wait(0.07):
                        return
                return
            if case == "ping_progress":
                for _ in range(5):
                    stream.sendall(frame(0x9, ping))
                    assert client_frame(stream, 0xa, masks) == ping
                    if stop.wait(0.07):
                        return
                stop.wait(2)
                return

            no_initial_ping = {"no_ping", "budget_limit", "closing_budget_limit",
                               "closing_budget_exceeded"}
            if case not in no_initial_ping:
                if case != "early_control":
                    prefix = frame(0xa, b"unsolicited") if case in {"unsolicited", "coalesced"} else b""
                    transmit(prefix + frame(0x9, ping), case == "split")
                assert client_frame(stream, 0xa, masks) == ping
            if case in {"wrong_pong", "missing_pong"}:
                if case == "wrong_pong":
                    wrong = bytes([token[0] ^ 1]) + token[1:]
                    stream.sendall(frame(0xa, wrong))
                stream.sendall(frame(0x1, message))
                stop.wait(2)
                return
            if case == "wrong_echo":
                stream.sendall(frame(0xa, token) + frame(0x1, b"wrong"))
                return
            if case == "duplicate_echo":
                stream.sendall(frame(0x1, message) * 2 + frame(0xa, token))
                return
            if case == "echo_first":
                stream.sendall(frame(0x1, message))
                # Correctly receiving the echo alone must not start Close.
                import select
                assert not select.select([stream], [], [], 0.05)[0], "client closed before matching Pong"
                data = frame(0xa, token)
            else:
                data = frame(0xa, token)
                if case == "duplicate_pong":
                    data += frame(0xa, token)
                if case == "budget_limit":
                    data += frame(0xa, b"noise") * 31
                data += frame(0x1, message)
            if case == "closing_ping":
                data += frame(0x9, b"\x00closing\xff")  # Sent before receiving client Close.
            if case == "closing_pong":
                data += frame(0xa, b"notice")
            if case in {"closing_budget_limit", "closing_budget_exceeded"}:
                data += frame(0xa, b"noise") * (31 if case == "closing_budget_limit" else 32)
            if case == "text_during_close":
                data += frame(0x1, message)
            transmit(data, case == "split")
            assert client_frame(stream, 0x8, masks) == b"\x03\xe8"
            if case == "closing_ping":
                assert client_frame(stream, 0xa, masks) == b"\x00closing\xff"
            if case in {"closing_budget_exceeded", "text_during_close", "missing_close"}:
                return
            if case == "idle_close":
                stop.wait(2)
                return
            tail = b"\x88\x02\x03\xe8"
            if case == "trailing_after_close":
                tail += frame(0x9, ping)
            transmit(tail, case == "split")
            if case == "idle_tcp":
                stop.wait(2)
                return
    except OSError as error:
        if case in positive:
            failures.append(error)
    except BaseException as error:
        failures.append(error)


worker = threading.Thread(target=peer)
worker.start()
started = time.monotonic()
try:
    result = subprocess.run([binary, str(port), message.decode(),
                             "600" if case in deadline_cases else "3000"],
                            capture_output=True, timeout=5)
    elapsed = time.monotonic() - started
    if case in positive:
        assert result.returncode == 0 and result.stdout == expected_output(), result
    else:
        assert result.returncode == 1 and not result.stdout, result
finally:
    stop.set()
    worker.join(4)
    listener.close()
assert not worker.is_alive() and not failures, failures
if case in deadline_cases:
    assert b"session deadline expired" in result.stderr, result
    assert 0.45 < elapsed < 2.5, elapsed
if case in {"budget_exceeded", "closing_budget_exceeded"}:
    assert b"Ping/Pong frame budget exceeded" in result.stderr, result
