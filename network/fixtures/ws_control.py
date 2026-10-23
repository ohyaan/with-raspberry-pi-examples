"""Finite Ping/Pong interleaving fixture on loopback, not full WebSocket."""
import argparse
import socket

from ws_handshake import accept_value
from ws_text import read_client_frame


def serve(port):
    with socket.socket() as listener:
        listener.settimeout(30)
        listener.bind(("127.0.0.1", port))
        listener.listen(1)
        port = listener.getsockname()[1]
        print(f"LISTEN {port}", flush=True)
        peer, _ = listener.accept()
        with peer:
            peer.settimeout(3)
            request = bytearray()
            while b"\r\n\r\n" not in request:
                part = peer.recv(min(128, 4097 - len(request)))
                if not part:
                    raise RuntimeError("incomplete opening handshake")
                request.extend(part)
                if len(request) > 4096:
                    raise RuntimeError("request exceeds 4096 bytes")
            lines = bytes(request).decode("ascii").split("\r\n")
            if lines[0] != "GET /ws HTTP/1.1" or lines[-2:] != ["", ""]:
                raise RuntimeError("request outside fixture profile")
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
            response = ("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                        f"Connection: Upgrade\r\nSec-WebSocket-Accept: {accept_value(key)}\r\n\r\n")
            peer.sendall(response.encode("ascii"))
            token = read_client_frame(peer, 0x9)
            if len(token) != 8:
                raise RuntimeError("expected the client's eight-byte Ping token")
            text = read_client_frame(peer, 0x1)
            if any(value < 32 or value >= 127 for value in text):
                raise RuntimeError("only printable ASCII text is supported here")
            check = b"\x00check\xff"
            peer.sendall(b"\x8a\x06notice" + bytes([0x89, len(check)]) + check)
            if read_client_frame(peer, 0xa) != check:
                raise RuntimeError("Pong did not preserve the binary Ping payload")
            # This Ping is on the wire before the server receives client Close.
            closing = b"closing"
            peer.sendall(bytes([0x8a, len(token)]) + token
                         + bytes([0x81, len(text)]) + text
                         + bytes([0x89, len(closing)]) + closing)
            if read_client_frame(peer, 0x8) != b"\x03\xe8":
                raise RuntimeError("expected client Close 1000")
            if read_client_frame(peer, 0xa) != closing:
                raise RuntimeError("expected Pong for the in-flight Ping")
            peer.sendall(b"\x88\x02\x03\xe8")
            print("matched_pong=yes close=1000", flush=True)



if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", type=int, choices=range(0, 65536), metavar="port")
    serve(parser.parse_args().port)
