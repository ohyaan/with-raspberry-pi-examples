"""Finite DTLS pair, independent OpenSSL CLI peers and UDP loss fixtures."""
from pathlib import Path
import os
import selectors
import socket
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "fixtures"))
import tls_echo

case = sys.argv[1]
directory_bin = Path(sys.argv[2])
client = directory_bin / "dtls_echo_client"
server = directory_bin / "dtls_echo_server"
message = b"Hello over DTLS.\n"
expected = b"verified=localhost protocol=DTLSv1.2\necho_bytes=17 close_notify=sent\n" + message


def until(pipe, marker, seconds=3):
    data = bytearray()
    limit = time.monotonic() + seconds
    with selectors.DefaultSelector() as ready:
        ready.register(pipe, selectors.EVENT_READ)
        while marker not in data:
            assert ready.select(max(0, limit - time.monotonic())), ("peer not ready", bytes(data))
            part = os.read(pipe.fileno(), 1)
            assert part, ("peer ended early", bytes(data))
            data.extend(part)
            assert len(data) < 65536
    return bytes(data)


def finish(process):
    if process.poll() is None:
        process.terminate()
    try:
        return process.communicate(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        return process.communicate(timeout=3)


def types(packet):
    found = []
    offset = 0
    while offset + 13 <= len(packet):
        found.append(packet[offset])
        offset += 13 + int.from_bytes(packet[offset + 11:offset + 13], "big")
    return found


with tempfile.TemporaryDirectory(prefix="network-dtls-test-") as temporary:
    certs = Path(temporary) / "certs"
    tls_echo.certificates(certs)
    ca = certs / "ca.pem"
    if case == "invalid":
        for args in ([], ["0", str(ca)], ["65536", str(ca)], ["x", str(ca)],
                     ["1", str(ca), ""], ["1", str(ca), "local host"],
                     ["1", str(ca), "localhost", "0"],
                     ["1", str(ca), "localhost", "10001"],
                     ["1", str(ca), "localhost", "x"],
                     ["1", str(ca), "localhost", "1", "extra"]):
            result = subprocess.run([client, *args], capture_output=True, timeout=3)
            assert result.returncode == 1 and not result.stdout, result
        for args in ([], ["65536", "x", "y"], ["x", "x", "y"],
                     ["0", "x", "y", "0"], ["0", "x", "y", "10001"],
                     ["0", "x", "y", "x"], ["0", "x", "y", "1", "extra"]):
            result = subprocess.run([server, *args], capture_output=True, timeout=3)
            assert result.returncode == 1 and not result.stdout, result
        sys.exit(0)
    if case in {"missing_ca", "bad_ca"}:
        ca = certs / "missing.pem"
        if case == "bad_ca":
            ca.write_text("not a certificate\n")
        result = subprocess.run([client, "1", str(ca)], capture_output=True, timeout=3)
        assert result.returncode == 1 and not result.stdout and b"load CA file" in result.stderr, result
        sys.exit(0)
    if case == "missing_key":
        result = subprocess.run([server, "0", str(certs / "server.pem"), str(certs / "missing.key")],
                                capture_output=True, timeout=3)
        assert result.returncode == 1 and not result.stdout, result
        sys.exit(0)
    if case == "key_mismatch":
        other = Path(temporary) / "other"
        tls_echo.certificates(other)
        result = subprocess.run([server, "0", str(certs / "server.pem"), str(other / "server.key")],
                                capture_output=True, timeout=3)
        assert result.returncode == 1 and not result.stdout, result
        sys.exit(0)
    if case.startswith("cli_server"):
        # Read the CLI's actual readiness line, not a sleep or guessed port.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reserved:
            reserved.bind(("127.0.0.1", 0))
            port = reserved.getsockname()[1]
        process = subprocess.Popen(["openssl", "s_server", "-dtls1_2", "-timeout",
                                    "-accept", f"127.0.0.1:{port}",
                                    "-cert", str(certs / "server.pem"), "-key", str(certs / "server.key"),
                                    "-naccept", "1", "-ign_eof"],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            observed = until(process.stdout, b"ACCEPT\n")
            reply = message if case == "cli_server" else b"x" * (18 if case == "cli_server_long" else 17) if case == "cli_server_wrong" or case == "cli_server_long" else b"short"
            process.stdin.write(reply)
            process.stdin.flush()
            result = subprocess.run([client, str(port), str(ca)], capture_output=True, timeout=5)
            if case == "cli_server":
                assert result.returncode == 0 and result.stdout == expected, result
                observed += until(process.stdout, message)
                assert message in observed
            else:
                assert result.returncode == 1 and not result.stdout and b"datagram mismatch" in result.stderr, result
        finally:
            finish(process)
        sys.exit(0)

    process = None
    raw_peer = None
    stop = threading.Event()
    worker = None
    counts = {"client_app": 0, "server_app": 0, "client_handshake": 0, "server_handshake": 0}
    failures = []
    sockets = []
    if case in {"idle", "noise", "refused"}:
        raw_peer = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sockets.append(raw_peer)
        raw_peer.bind(("127.0.0.1", 0))
        port = raw_peer.getsockname()[1]
        if case == "refused":
            raw_peer.close()
        else:
            raw_peer.settimeout(0.1)
            def discard():
                while not stop.is_set():
                    try:
                        _, address = raw_peer.recvfrom(65536)
                        if case == "noise":
                            raw_peer.sendto(b"not a DTLS record", address)
                    except socket.timeout:
                        continue
                    except OSError:
                        return
            worker = threading.Thread(target=discard)
            worker.start()
    else:
        budget = "600" if case in {"server_idle", "server_bad_udp"} else "7000"
        process = subprocess.Popen([server, "0", str(certs / "server.pem"),
                                    str(certs / "server.key"), budget],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        announcement = until(process.stdout, b"\n").split()
        assert announcement[0] == b"LISTEN", announcement
        port = int(announcement[1])
        if case in {"server_idle", "server_bad_udp"}:
            if case == "server_bad_udp":
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as peer:
                    peer.sendto(b"not DTLS", ("127.0.0.1", port))
            output, error = process.communicate(timeout=3)
            assert process.returncode == 1 and not output and b"deadline" in error, (output, error)
            sys.exit(0)
        if case in {"cli_client", "cli_client_binary", "cli_client_limit", "cli_client_oversized"}:
            payload = (b"\x00binary\xff\n" if case == "cli_client_binary" else b"x" * 256 if case == "cli_client_limit"
                       else b"x" * 257 if case == "cli_client_oversized" else message)
            peer = subprocess.Popen(["openssl", "s_client", "-dtls1_2", "-quiet",
                                     "-connect", f"127.0.0.1:{port}", "-CAfile", str(ca),
                                     "-verify_hostname", "localhost", "-verify_return_error",
                                     "-servername", "localhost"],
                                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                peer.stdin.write(payload)
                peer.stdin.flush()
                if case != "cli_client_oversized":
                    echoed = until(peer.stdout, payload)
                    assert echoed == payload, echoed
                output, error = process.communicate(timeout=4)
                if case == "cli_client_oversized":
                    assert process.returncode == 1 and b"1..256-byte policy" in error, (output, error)
                else:
                    assert process.returncode == 0 and f"echo_bytes={len(payload)} ".encode() in output, (output, error)
            finally:
                finish(peer)
                finish(process)
            sys.exit(0)
        if case == "conflict":
            result = subprocess.run([server, str(port), str(certs / "server.pem"),
                                     str(certs / "server.key")], capture_output=True, timeout=3)
            finish(process)
            assert result.returncode == 1 and b"bind" in result.stderr and not result.stdout, result
            sys.exit(0)
        # A loopback UDP relay can drop ciphertext datagrams without doing crypto.
        front = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        back = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sockets.extend([front, back])
        front.bind(("127.0.0.1", 0))
        back.bind(("127.0.0.1", 0))
        server_port = port
        port = front.getsockname()[1]
        def relay():
            address = None
            dropped_client = dropped_server = False
            try:
                with selectors.DefaultSelector() as ready:
                    ready.register(front, selectors.EVENT_READ)
                    ready.register(back, selectors.EVENT_READ)
                    while not stop.is_set():
                        for event, _ in ready.select(0.05):
                            packet, source = event.fileobj.recvfrom(65536)
                            tags = types(packet)
                            if event.fileobj is front:
                                address = source
                                counts["client_app"] += tags.count(23)
                                counts["client_handshake"] += tags.count(22)
                                if case == "drop_client_hello" and 22 in tags and not dropped_client:
                                    dropped_client = True
                                    continue
                                if case == "drop_client_app" and 23 in tags:
                                    continue
                                back.sendto(packet, ("127.0.0.1", server_port))
                            else:
                                counts["server_app"] += tags.count(23)
                                counts["server_handshake"] += tags.count(22)
                                if case == "drop_server_flight" and 22 in tags and not dropped_server:
                                    dropped_server = True
                                    continue
                                if case == "drop_server_app" and (23 in tags or 21 in tags):
                                    continue
                                assert address is not None
                                front.sendto(packet, address)
            except BaseException as error:
                failures.append(error)
        worker = threading.Thread(target=relay)
        worker.start()
    if case == "wrong_ca":
        other = Path(temporary) / "other"
        tls_echo.certificates(other)
        ca = other / "ca.pem"
    positive = {"pair", "drop_client_hello", "drop_server_flight"}
    deadline_cases = {"idle", "noise", "drop_client_app", "drop_server_app"}
    budget = "4000" if case in positive else "600"
    try:
        started = time.monotonic()
        result = subprocess.run([client, str(port), str(ca),
                                 "wrong.example" if case == "wrong_host" else "localhost", budget],
                                capture_output=True, timeout=6)
        elapsed = time.monotonic() - started
        if case in positive:
            assert result.returncode == 0 and result.stdout == expected, result
        else:
            assert result.returncode == 1 and not result.stdout, result
        if case in deadline_cases:
            assert b"deadline" in result.stderr and 0.45 <= elapsed < 2.5, (result, elapsed)
        if case in {"wrong_ca", "wrong_host"}:
            assert counts["client_app"] == 0 and b"TLS operation failed" in result.stderr, (counts, result)
        if case == "drop_client_hello":
            assert counts["client_handshake"] >= 2 and elapsed >= 0.85, (counts, elapsed)
        if case == "drop_server_flight":
            assert counts["server_handshake"] >= 2 and elapsed >= 0.85, (counts, elapsed)
        if case in {"drop_client_app", "drop_server_app"}:
            assert counts["client_app"] == 1, counts
        if process is not None and case in positive | {"drop_server_app"}:
            output, error = process.communicate(timeout=3)
            assert process.returncode == 0 and b"echo_bytes=17 " in output, (output, error)
    finally:
        stop.set()
        if worker is not None:
            worker.join(3)
            assert not worker.is_alive()
        for item in sockets:
            item.close()
        if process is not None:
            finish(process)
    assert not failures, failures
