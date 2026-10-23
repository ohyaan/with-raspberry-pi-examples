"""One ASCII short-frame echo and Close exchange on loopback, not full WebSocket."""
import argparse
import socket

from ws_handshake import accept_value


def read_exact(peer, count):
    result = bytearray()
    while len(result) < count:
        part = peer.recv(count - len(result))
        if not part:
            raise RuntimeError("TCP ended before complete input")
        result.extend(part)
    return bytes(result)


def read_client_frame(peer, expected_opcode):
    first, second = read_exact(peer, 2)
    if first != 0x80 | expected_opcode or not second & 0x80 or second & 127 > 125:
        raise RuntimeError("expected a final masked short client frame")
    mask = read_exact(peer, 4)
    payload = read_exact(peer, second & 127)
    return bytes(value ^ mask[index % 4] for index, value in enumerate(payload))


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
            text = read_client_frame(peer, 0x1)
            if any(value < 32 or value >= 127 for value in text):
                raise RuntimeError("only printable ASCII is supported here")
            peer.sendall(bytes([0x81, len(text)]) + text)
            if read_client_frame(peer, 0x8) != b"\x03\xe8":
                raise RuntimeError("expected Close 1000 with no reason")
            peer.sendall(b"\x88\x02\x03\xe8")
            print("close=1000", flush=True)
            # Returning closes TCP after both Close frames were exchanged.


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", type=int, choices=range(0, 65536), metavar="port")
    serve(parser.parse_args().port)
