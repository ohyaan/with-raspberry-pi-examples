"""One bodyless GET over verified-client-compatible TLS on IPv4 loopback."""
import argparse
from pathlib import Path
import socket

import tls_echo


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
                    raise RuntimeError("HTTP/1.1 was not negotiated")
                request = bytearray()
                while b"\r\n\r\n" not in request:
                    part = peer.recv(min(128, 4097 - len(request)))
                    if not part:
                        raise RuntimeError("incomplete request")
                    request.extend(part)
                    if len(request) > 4096:
                        raise RuntimeError("request exceeds 4096 bytes")
                lines = bytes(request).split(b"\r\n")
                first = lines[0].split(b" ")
                if (len(first) != 3 or first[0] != b"GET" or first[2] != b"HTTP/1.1"
                        or lines[1:] != [f"Host: localhost:{port}".encode(),
                                        b"Connection: close", b"", b""]):
                    raise RuntimeError("request outside fixture profile")
                found = first[1] == b"/hello.txt"
                body = b"Hello over HTTPS.\n" if found else b"Not found\n"
                status = b"200 OK" if found else b"404 Not Found"
                response = (b"HTTP/1.1 " + status + b"\r\nContent-Type: text/plain\r\n"
                            + f"Content-Length: {len(body)}\r\n".encode()
                            + b"Connection: close\r\n\r\n" + body)
                peer.sendall(response)
                peer.unwrap().close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", type=int, choices=range(0, 65536), metavar="port")
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    serve(args.port, args.directory.resolve())
