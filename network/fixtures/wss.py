"""One fragmented ASCII echo over loopback TLS 1.3 with HTTP/1.1 ALPN."""
import argparse
import socket
from pathlib import Path

import tls_echo

from ws_handshake import accept_value
from ws_text import read_exact


def read_frame(peer, opcode, final=True):
    first, second = read_exact(peer, 2)
    if first != (0x80 if final else 0) | opcode or not second & 128:
        raise RuntimeError("unexpected client frame flags/opcode/mask")
    marker = second & 127
    count = marker
    if marker >= 126:
        count = int.from_bytes(read_exact(peer, 2 if marker == 126 else 8), "big")
        if (marker == 126 and count < 126) or (marker == 127 and count < 65536):
            raise RuntimeError("nonminimal length")
    if count > 65536 or (opcode >= 8 and count > 125):
        raise RuntimeError("frame outside fixture policy")
    mask = read_exact(peer, 4)
    payload = read_exact(peer, count)
    return bytes(value ^ mask[i % 4] for i, value in enumerate(payload))


def frame(opcode, payload, final=True):
    count = len(payload)
    if count <= 125:
        header = bytes([(0x80 if final else 0) | opcode, count])
    elif count <= 65535:
        header = bytes([(0x80 if final else 0) | opcode, 126]) + count.to_bytes(2, "big")
    else:
        header = bytes([(0x80 if final else 0) | opcode, 127]) + count.to_bytes(8, "big")
    return header + payload


def serve(port, directory):
    context = tls_echo.context(directory)
    context.set_alpn_protocols(["http/1.1"])
    with socket.socket() as listener:
        listener.settimeout(30)
        listener.bind(("127.0.0.1", port))
        listener.listen(1)
        port = listener.getsockname()[1]
        print(f"LISTEN {port}", flush=True)
        raw, _ = listener.accept()
        with raw:
            raw.settimeout(3)
            with context.wrap_socket(raw, server_side=True) as peer:
                if peer.selected_alpn_protocol() != "http/1.1":
                    raise RuntimeError("HTTP/1.1 ALPN required")
                request = bytearray()
                while not request.endswith(b"\r\n\r\n"):
                    request.extend(read_exact(peer, 1))
                    if len(request) > 4096:
                        raise RuntimeError("request exceeds 4096 bytes")
                lines = request.decode("ascii").split("\r\n")
                if lines[0] != "GET /ws HTTP/1.1":
                    raise RuntimeError("unexpected request")
                fields = {}
                for line in lines[1:-2]:
                    name, value = line.split(":", 1)
                    if name.lower() in fields:
                        raise RuntimeError("duplicate request field")
                    fields[name.lower()] = value.strip()
                key = fields.pop("sec-websocket-key")
                if fields != {"host": f"localhost:{port}", "upgrade": "websocket",
                              "connection": "Upgrade", "sec-websocket-version": "13"}:
                    raise RuntimeError("unexpected handshake fields")
                peer.sendall(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                              f"Connection: Upgrade\r\nSec-WebSocket-Accept: {accept_value(key)}\r\n\r\n").encode())
                token = read_frame(peer, 9)
                if len(token) != 8:
                    raise RuntimeError("expected eight-byte Ping")
                text = (read_frame(peer, 1, False) + read_frame(peer, 0, False)
                        + read_frame(peer, 0, True))
                if len(text) > 65536:
                    raise RuntimeError("message exceeds fixture policy")
                if any(c < 32 or c >= 127 for c in text):
                    raise RuntimeError("only printable ASCII is supported")
                middle = len(text) // 2
                peer.sendall(frame(1, text[:middle], False) + frame(9, b"\x00check\xff"))
                if read_frame(peer, 10) != b"\x00check\xff":
                    raise RuntimeError("incorrect Pong")
                peer.sendall(frame(10, token) + frame(0, b"", False)
                             + frame(0, text[middle:], True))
                if read_frame(peer, 8) != b"\x03\xe8":
                    raise RuntimeError("expected Close 1000")
                peer.sendall(frame(8, b"\x03\xe8"))
                peer.unwrap().close()
                print(f"echo_bytes={len(text)} close=1000 tls_close_notify=yes", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", type=int, choices=range(0, 65536), metavar="port")
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    serve(args.port, args.directory.resolve())
