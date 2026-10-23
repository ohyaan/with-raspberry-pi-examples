"""Independent packet oracle, real DTLS peers and bounded retry/loss checks."""
from pathlib import Path
import os
import selectors
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "fixtures"))
import tls_echo

case = sys.argv[1]
binaries = Path(sys.argv[2])
client = binaries / "dtls_ids_client"
server = binaries / "dtls_ids_server"
request = struct.pack("!BBII", 1, 1, 42, 7)
response = struct.pack("!BBIII", 1, 2, 42, 7, 1)


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


def expected(attempts, ignored=0):
    return (f"verified=localhost protocol=DTLSv1.2\n"
            f"id=42 value=7 executions=1 attempts={attempts} ignored={ignored}\n").encode()


def records(packet):
    result = []
    offset = 0
    while offset + 13 <= len(packet):
        result.append((packet[offset], packet[offset + 3:offset + 11]))
        offset += 13 + int.from_bytes(packet[offset + 11:offset + 13], "big")
    return result


with tempfile.TemporaryDirectory(prefix="network-dtls-ids-test-") as temporary:
    certs = Path(temporary) / "certs"
    tls_echo.certificates(certs)
    ca = certs / "ca.pem"
    cert = certs / "server.pem"
    key = certs / "server.key"
    if case == "invalid":
        for args in ([], ["0", str(ca)], ["65536", str(ca)], ["x", str(ca)],
                     ["1", str(ca), ""], ["1", str(ca), "local host"],
                     ["1", str(ca), "localhost", "0"], ["1", str(ca), "localhost", "10001"],
                     ["1", str(ca), "localhost", "x"], ["1", str(ca), "localhost", "1", "extra"]):
            result = subprocess.run([client, *args], capture_output=True, timeout=3)
            assert result.returncode == 1 and not result.stdout, result
        for args in ([], ["65536", str(cert), str(key)], ["x", str(cert), str(key)],
                     ["0", str(cert), str(key), "0"], ["0", str(cert), str(key), "10001"],
                     ["0", str(cert), str(key), "x"], ["0", str(cert), str(key), "1", "extra"]):
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
    if case in {"missing_key", "key_mismatch"}:
        key = certs / "missing.key"
        if case == "key_mismatch":
            other = Path(temporary) / "other"
            tls_echo.certificates(other)
            key = other / "server.key"
        result = subprocess.run([server, "0", str(cert), str(key)], capture_output=True, timeout=3)
        assert result.returncode == 1 and not result.stdout, result
        sys.exit(0)
    if case.startswith("reply_"):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reserved:
            reserved.bind(("127.0.0.1", 0))
            port = reserved.getsockname()[1]
        peer = subprocess.Popen(["openssl", "s_server", "-dtls1_2", "-timeout",
                                 "-accept", f"127.0.0.1:{port}", "-cert", str(cert),
                                 "-key", str(key), "-naccept", "1", "-ign_eof"],
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        command = None
        try:
            until(peer.stdout, b"ACCEPT\n")
            command = subprocess.Popen([client, str(port), str(ca)],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            until(peer.stdout, request)
            bad = {"reply_wrong_value": struct.pack("!BBIII", 1, 2, 42, 8, 1),
                   "reply_wrong_executions": struct.pack("!BBIII", 1, 2, 42, 7, 2),
                   "reply_bad_version": bytes([2]) + response[1:],
                   "reply_bad_kind": response[:1] + bytes([1]) + response[2:],
                   "reply_short": response[:-1], "reply_long": response + b"x"}
            stale = struct.pack("!BBIII", 1, 2, 41, 7, 1)
            if case in {"reply_stale_then_valid", "reply_stale_only"}:
                peer.stdin.write(stale)
                peer.stdin.flush()
                until(peer.stdout, request)
                if case == "reply_stale_only":
                    peer.stdin.write(stale)
                    peer.stdin.flush()
                    until(peer.stdout, request)
                    peer.stdin.write(stale)
                    peer.stdin.flush()
                else:
                    peer.stdin.write(response)
                    peer.stdin.flush()
            else:
                peer.stdin.write(bad.get(case, response))
                peer.stdin.flush()
            output, error = command.communicate(timeout=4)
            if case in {"reply_valid", "reply_stale_then_valid"}:
                assert command.returncode == 0 and output == expected(
                    2 if case == "reply_stale_then_valid" else 1,
                    1 if case == "reply_stale_then_valid" else 0), (output, error)
            else:
                assert command.returncode == 1 and not output and b"outcome unknown" in error, (output, error)
                if case == "reply_stale_only":
                    assert b"after 3 attempts" in error, error
        finally:
            if command is not None:
                finish(command)
            finish(peer)
        sys.exit(0)

    process = subprocess.Popen([server, "0", str(cert), str(key),
                                "600" if case == "server_idle" else "7000"],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    worker = None
    stop = threading.Event()
    sockets = []
    failures = []
    counts = {"client_app": 0, "server_app": 0}
    sequences = []
    try:
        line = until(process.stdout, b"\n").split()
        assert line[0] == b"LISTEN", line
        server_port = int(line[1])
        if case == "server_idle":
            output, error = process.communicate(timeout=3)
            assert process.returncode == 1 and not output and b"deadline" in error, (output, error)
            sys.exit(0)
        if case == "conflict":
            result = subprocess.run([server, str(server_port), str(cert), str(key)], capture_output=True, timeout=3)
            assert result.returncode == 1 and not result.stdout and b"bind" in result.stderr, result
            sys.exit(0)
        if case.startswith("cli_"):
            peer = subprocess.Popen(["openssl", "s_client", "-dtls1_2", "-quiet", "-no_ign_eof",
                                     "-connect", f"127.0.0.1:{server_port}", "-CAfile", str(ca),
                                     "-verify_hostname", "localhost", "-verify_return_error",
                                     "-servername", "localhost"],
                                    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                malformed = {"cli_amount_zero": struct.pack("!BBII", 1, 1, 42, 0),
                             "cli_amount_over": struct.pack("!BBII", 1, 1, 42, 101),
                             "cli_short_request": request[:-1], "cli_long_request": request + b"x",
                             "cli_bad_version": bytes([2]) + request[1:],
                             "cli_bad_kind": request[:1] + bytes([2]) + request[2:]}
                first = malformed.get(case, struct.pack("!BBII", 1, 1, 42, 100) if case == "cli_amount_max" else request)
                peer.stdin.write(first)
                peer.stdin.flush()
                if case not in malformed:
                    reply = struct.pack("!BBIII", 1, 2, 42, 100, 1) if case == "cli_amount_max" else response
                    assert until(peer.stdout, reply) == reply
                if case in {"cli_cached", "cli_conflict", "cli_new_id", "cli_budget"}:
                    limit = 16 if case == "cli_budget" else 1
                    for index in range(limit):
                        next_request = (struct.pack("!BBII", 1, 1, 42, 8) if case == "cli_conflict"
                                        else struct.pack("!BBII", 1, 1, 43, 7) if case == "cli_new_id" else request)
                        peer.stdin.write(next_request)
                        peer.stdin.flush()
                        if case == "cli_cached" or (case == "cli_budget" and index < 15):
                            assert until(peer.stdout, response) == response
                success = case in {"cli_cached", "cli_amount_max"}
                if success:
                    peer.stdin.close()
                    peer.stdin = None
                    peer.communicate(timeout=3)
                output, error = process.communicate(timeout=3)
                assert output.count(b"APPLIED ") == (0 if case in malformed else 1), (output, error)
                if success:
                    assert process.returncode == 0 and b"executions=1 " in output, (output, error)
                else:
                    assert process.returncode == 1, (output, error)
                    required = {"cli_conflict": b"different request bytes",
                                "cli_new_id": b"only one request ID", "cli_budget": b"request-count budget"}
                    if case in required:
                        assert required[case] in error, error
            finally:
                finish(peer)
            sys.exit(0)
        front = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        back = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sockets.extend([front, back])
        front.bind(("127.0.0.1", 0))
        back.bind(("127.0.0.1", 0))
        port = front.getsockname()[1]

        def relay():
            address = None
            first_hello = True
            held = None
            try:
                with selectors.DefaultSelector() as ready:
                    ready.register(front, selectors.EVENT_READ)
                    ready.register(back, selectors.EVENT_READ)
                    while True:
                        events = ready.select(0.05)
                        if stop.is_set() and not events:
                            break  # Drain queued packets before final assertions.
                        for event, _ in events:
                            packet, source = event.fileobj.recvfrom(65536)
                            entries = records(packet)
                            tags = [tag for tag, _ in entries]
                            if event.fileobj is front:
                                address = source
                                if 22 in tags:
                                    if case == "handshake_idle":
                                        continue
                                    if case == "handshake_loss" and first_hello:
                                        first_hello = False
                                        continue
                                if 23 in tags:
                                    counts["client_app"] += tags.count(23)
                                    sequences.extend(seq for tag, seq in entries if tag == 23)
                                    if case == "drop_all_requests" or (case == "drop_first_request" and counts["client_app"] == 1):
                                        continue
                                    if case == "replay_record" and counts["client_app"] == 1:
                                        back.sendto(packet, ("127.0.0.1", server_port))
                                back.sendto(packet, ("127.0.0.1", server_port))
                            else:
                                if 23 in tags:
                                    counts["server_app"] += tags.count(23)
                                    if case in {"drop_all_replies", "short_deadline"}:
                                        continue
                                    if case == "drop_first_reply" and counts["server_app"] == 1:
                                        continue
                                    if case == "drop_two_replies" and counts["server_app"] <= 2:
                                        continue
                                    if case == "delay_first_reply":
                                        if held is None:
                                            held = packet
                                            continue
                                        packet = held  # Only the earlier attempt's reply is released.
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
        successes = {"pair": 1, "drop_first_request": 2, "drop_first_reply": 2,
                     "drop_two_replies": 3, "delay_first_reply": 2,
                     "replay_record": 1, "handshake_loss": 1}
        started = time.monotonic()
        result = subprocess.run([client, str(port), str(ca),
                                 "wrong.example" if case == "wrong_host" else "localhost",
                                 "600" if case in {"short_deadline", "handshake_idle"} else "4000"],
                                capture_output=True, timeout=6)
        elapsed = time.monotonic() - started
        if case in successes:
            assert result.returncode == 0 and result.stdout == expected(successes[case]), result
            output, error = process.communicate(timeout=3)
            assert process.returncode == 0, (output, error)
        else:
            assert result.returncode == 1 and not result.stdout, result
            output, error = finish(process)
        if case in {"drop_all_requests", "drop_all_replies", "short_deadline"}:
            assert b"outcome unknown" in result.stderr, result
        if case in {"drop_all_requests", "drop_all_replies"}:
            assert b"after 3 attempts" in result.stderr and 1.55 <= elapsed < 3.3, (elapsed, result)
        if case in {"short_deadline", "handshake_idle"}:
            assert b"deadline" in result.stderr and 0.45 <= elapsed < 2.5, (elapsed, result)
        applied = 0 if case in {"drop_all_requests", "wrong_ca", "wrong_host", "handshake_idle"} else 1
        assert output.count(b"APPLIED ") == applied, (output, error)
        requests_seen = {"pair": 1, "drop_first_request": 1, "drop_first_reply": 2,
                         "drop_two_replies": 3, "delay_first_reply": 2,
                         "replay_record": 1, "handshake_loss": 1}.get(case)
        if requests_seen is not None:
            assert f"requests={requests_seen} executions=1 value=7 ".encode() in output, (output, error)
        if case in {"wrong_ca", "wrong_host"}:
            assert b"TLS operation failed" in result.stderr, result
    finally:
        stop.set()
        if worker is not None:
            worker.join(3)
            assert not worker.is_alive()
        for item in sockets:
            item.close()
        finish(process)
    assert not failures, failures
    assert len(sequences) == len(set(sequences)), sequences  # App retries are new DTLS records.
    if case in {"wrong_ca", "wrong_host", "handshake_idle"}:
        assert counts["client_app"] == 0, counts
    if case in {"drop_all_requests", "drop_all_replies"}:
        assert counts["client_app"] == 3, counts
    if case in {"drop_first_request", "drop_first_reply", "drop_two_replies",
                "drop_all_requests", "drop_all_replies", "delay_first_reply", "replay_record"}:
        print(result.stdout.decode(), end="")
        print(result.stderr.decode(), end="")
        print(output.decode(), end="")
        print(f"relay_client_app_records={counts['client_app']}")
