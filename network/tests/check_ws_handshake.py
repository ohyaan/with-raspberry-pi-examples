"""Independent HTTP byte oracle for the WebSocket handshake-only client."""
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
binary = Path(sys.argv[2]) / "ws_handshake_client"
positive = {"combined", "split", "mixed_case", "connection_list", "whitespace",
            "unused_header", "repeated_unused", "header_limit", "coalesced_prefix", "fresh_key"}


def inspect_request(peer, port):
    request = bytearray()
    while b"\r\n\r\n" not in request:
        part = peer.recv(128)
        assert part, "request ended before its header terminator"
        request.extend(part)
        assert len(request) <= 4096
    lines = bytes(request).decode("ascii").split("\r\n")
    assert lines[0] == "GET /ws HTTP/1.1" and lines[-2:] == ["", ""], lines
    fields = {}
    for line in lines[1:-2]:
        name, value = line.split(":", 1)
        assert name.lower() not in fields
        fields[name.lower()] = value.strip()
    key = fields.pop("sec-websocket-key")
    assert len(key) == 24 and len(base64.b64decode(key, validate=True)) == 16, key
    assert fields == {"host": f"localhost:{port}", "upgrade": "websocket",
                      "connection": "Upgrade", "sec-websocket-version": "13"}, fields
    # No premature half-close or application bytes while the response is pending.
    peer.setblocking(False)
    try:
        received = peer.recv(1, socket.MSG_PEEK)
    except BlockingIOError:
        pass
    else:
        raise AssertionError(f"write side closed or extra data before 101: {received!r}")
    finally:
        peer.settimeout(3)
    return key


if case == "invalid":
    for args in ([], ["0"], ["65536"], ["12x"], ["-1"], ["1", "0"],
                 ["1", "10001"], ["1", "x"], ["1", "3000", "extra"]):
        result = subprocess.run([binary, *args], capture_output=True, timeout=3)
        assert result.returncode == 1 and not result.stdout, result
    sys.exit(0)
if case == "fixture":
    fixture = Path(__file__).resolve().parents[1] / "fixtures/ws_handshake.py"
    process = subprocess.Popen([sys.executable, str(fixture), "0"],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(process.stdout, selectors.EVENT_READ)
            assert ready.select(3), "fixture did not announce its port"
        announcement = process.stdout.readline().split()
        assert announcement[0] == "LISTEN", announcement
        result = subprocess.run([binary, announcement[1]], capture_output=True, timeout=5)
        assert result.returncode == 0, result
        assert result.stdout == (b"status=101 upgrade=websocket\nbuffered_frame_bytes=0\n"
                                 b"buffered_prefix_hex=\n"), result
        output, error = process.communicate(timeout=3)
        assert process.returncode == 0 and "without a WebSocket Close frame" in output, error
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
failures = []
keys = []
stop = threading.Event()
frame = b"\x81\x05hello" if case == "coalesced_prefix" else b""
repetitions = 4 if case == "fresh_key" else 1


def peer():
    try:
        for _ in range(repetitions):
            stream, _ = listener.accept()
            with stream:
                stream.settimeout(3)
                key = inspect_request(stream, port)
                keys.append(key)
                accept = base64.b64encode(hashlib.sha1(
                    (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
                fields = ["Upgrade: websocket", "Connection: Upgrade", f"Sec-WebSocket-Accept: {accept}"]
                if case == "mixed_case":
                    fields = ["uPgRaDe: WeBsOcKeT", "cOnNeCtIoN: uPgRaDe", f"sEc-WeBsOcKeT-aCcEpT: {accept}"]
                if case == "connection_list":
                    fields[1] = "Connection: keep-alive, UpGrAdE"
                if case == "whitespace":
                    fields = ["Upgrade:\t websocket \t", "Connection:\t Upgrade \t",
                              f"Sec-WebSocket-Accept:\t {accept} \t"]
                if case in ("unused_header", "repeated_unused"):
                    fields.append("X-Note: ignored")
                    if case == "repeated_unused":
                        fields.append("X-Note: also ignored")
                if case == "wrong_accept":
                    fields[2] = "Sec-WebSocket-Accept: " + "X" * 28
                if case == "sample_accept":
                    fields[2] = "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo="
                for suffix, index in (("accept", 2), ("upgrade", 0), ("connection", 1)):
                    if case == "missing_" + suffix:
                        fields.pop(index)
                    if case == "duplicate_" + suffix:
                        fields.append(fields[index])
                changes = {
                    "wrong_upgrade": (0, "Upgrade: h2c"),
                    "wrong_connection": (1, "Connection: keep-alive"),
                    "substring_connection": (1, "Connection: xupgrade"),
                    "empty_token": (1, "Connection: Upgrade,,keep-alive"),
                    "trailing_token": (1, "Connection: Upgrade,"),
                    "invalid_token": (1, "Connection: Upgrade, bad token"),
                }
                if case in changes:
                    index, value = changes[case]
                    fields[index] = value
                additions = {"protocol": "Sec-WebSocket-Protocol: chat",
                             "extension": "Sec-WebSocket-Extensions: permessage-deflate",
                             "content_length": "Content-Length: 0",
                             "transfer_encoding": "Transfer-Encoding: chunked",
                             "malformed_name": "Bad Field: value",
                             "malformed_value": "X: \x00",
                             "folded_header": "X: a\r\n b"}
                if case in additions:
                    fields.append(additions[case])
                status = "HTTP/1.1 101 Switching Protocols"
                if case == "bad_status":
                    status = "HTTP/1.1 200 OK"
                if case == "bad_version":
                    status = "HTTP/1.0 101 Switching Protocols"
                response = (status + "\r\n" + "\r\n".join(fields) + "\r\n\r\n").encode()
                if case in ("header_limit", "oversized_headers"):
                    prefix = response[:-2] + b"X-Pad: "
                    size = 4096 if case == "header_limit" else 4097
                    response = prefix + b"x" * (size - len(prefix) - 4) + b"\r\n\r\n"
                    assert len(response) == size
                if case == "truncated":
                    stream.sendall(response[:-1])
                    return
                if case == "idle":
                    stop.wait(2)
                    return
                if case in ("split", "trickle"):
                    for byte in response:
                        stream.sendall(bytes([byte]))
                        if stop.wait(0.10 if case == "trickle" else 0.001):
                            return
                else:
                    stream.sendall(response + frame)
                if case in positive:
                    # Client finishes at the verified empty line, without peer EOF.
                    try:
                        following = stream.recv(1)
                    except ConnectionResetError:
                        if case != "coalesced_prefix":
                            raise
                        # The probe can close with unread queued frame bytes.
                        following = b""
                    assert following == b"", "probe must not send a frame yet"
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
        result = subprocess.run([binary, str(port), "600" if case in ("idle", "trickle") else "3000"],
                                capture_output=True, timeout=5)
        if case in positive:
            assert result.returncode == 0, result
            lines = result.stdout.decode().splitlines()
            assert len(lines) == 3 and lines[0] == "status=101 upgrade=websocket", lines
            count = int(lines[1].removeprefix("buffered_frame_bytes="))
            prefix = bytes.fromhex(lines[2].removeprefix("buffered_prefix_hex="))
            assert count == len(prefix) <= len(frame) and prefix == frame[:count], lines
        else:
            assert result.returncode == 1 and not result.stdout, result
    elapsed = time.monotonic() - started
finally:
    stop.set()
    worker.join(4)
    listener.close()
assert not worker.is_alive() and not failures, failures
if case == "fresh_key":
    assert len(set(keys)) == 4, keys  # A regression check, not an entropy audit.
if case in ("idle", "trickle"):
    assert b"handshake deadline expired" in result.stderr, result
    assert 0.45 < elapsed < 2.5, elapsed
