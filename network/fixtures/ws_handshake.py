"""Loopback handshake fixture; no WebSocket frame/Close implementation yet."""
import argparse
import base64
import hashlib
import socket


def accept_value(key):
    if len(base64.b64decode(key, validate=True)) != 16:
        raise ValueError("key must encode 16 bytes")
    challenge = (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")
    return base64.b64encode(hashlib.sha1(challenge).digest()).decode("ascii")


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
                    raise RuntimeError("incomplete handshake request")
                request.extend(part)
                if len(request) > 4096:
                    raise RuntimeError("request exceeds 4096 bytes")
            lines = bytes(request).decode("ascii").split("\r\n")
            if lines[0] != "GET /ws HTTP/1.1" or lines[-2:] != ["", ""]:
                raise RuntimeError("request outside fixture profile")
            fields = {}
            for line in lines[1:-2]:
                name, value = line.split(":", 1)
                name = name.lower()
                if name in fields:
                    raise RuntimeError("duplicate request field")
                fields[name] = value.strip()
            key = fields.pop("sec-websocket-key")
            if fields != {"host": f"localhost:{port}", "upgrade": "websocket",
                          "connection": "Upgrade", "sec-websocket-version": "13"}:
                raise RuntimeError("unexpected handshake fields")
            response = ("HTTP/1.1 101 Switching Protocols\r\n"
                        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                        f"Sec-WebSocket-Accept: {accept_value(key)}\r\n\r\n")
            peer.sendall(response.encode("ascii"))
            if peer.recv(1):
                raise RuntimeError("this fixture expects the handshake-only probe to disconnect")
            print("probe disconnected without a WebSocket Close frame", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", type=int, choices=range(0, 65536), metavar="port")
    serve(parser.parse_args().port)
