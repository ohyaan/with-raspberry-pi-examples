"""Local TLS 1.3 fixture, not a production server or a public CA."""
import argparse
from pathlib import Path
import socket
import ssl
import subprocess


def certificates(directory):
    directory.mkdir(mode=0o700, parents=True, exist_ok=False)

    def openssl(*arguments):
        subprocess.run(["openssl", *arguments], cwd=directory, check=True,
                       capture_output=True, timeout=10)

    openssl("req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256",
            "-noenc", "-keyout", "ca.key", "-out", "ca.pem", "-days", "2",
            "-subj", "/CN=Network Course Practice CA",
            "-addext", "basicConstraints=critical,CA:TRUE",
            "-addext", "keyUsage=critical,keyCertSign,cRLSign")
    openssl("req", "-new", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256",
            "-noenc", "-keyout", "server.key", "-out", "server.csr",
            "-subj", "/CN=localhost")
    (directory / "server.ext").write_text(
        "basicConstraints=critical,CA:FALSE\n"
        "keyUsage=critical,digitalSignature\n"
        "extendedKeyUsage=serverAuth\n"
        "subjectAltName=DNS:localhost\n", encoding="ascii")
    openssl("x509", "-req", "-in", "server.csr", "-CA", "ca.pem", "-CAkey", "ca.key",
            "-CAcreateserial", "-out", "server.pem", "-days", "2",
            "-extfile", "server.ext")
    for name in ("ca.key", "server.key"):
        (directory / name).chmod(0o600)


def context(directory):
    result = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    result.minimum_version = ssl.TLSVersion.TLSv1_3
    result.load_cert_chain(directory / "server.pem", directory / "server.key")
    return result


def serve(port, directory):
    tls_context = context(directory)
    with socket.socket() as listener:
        listener.settimeout(30)
        listener.bind(("127.0.0.1", port))
        listener.listen(1)
        print(f"LISTEN {listener.getsockname()[1]}", flush=True)
        raw, _ = listener.accept()
        with raw:
            raw.settimeout(3)
            with tls_context.wrap_socket(raw, server_side=True) as peer:
                request = bytearray()
                while len(request) < 16:
                    part = peer.recv(16 - len(request))
                    if not part:
                        raise RuntimeError("request ended before 16 bytes")
                    request.extend(part)
                peer.sendall(request)
                peer.unwrap().close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    certs = commands.add_parser("certs")
    certs.add_argument("directory", type=Path)
    server = commands.add_parser("serve")
    server.add_argument("port", type=int, choices=range(0, 65536), metavar="port")
    server.add_argument("directory", type=Path)
    args = parser.parse_args()
    if args.command == "certs":
        certificates(args.directory.resolve())
    else:
        serve(args.port, args.directory.resolve())
