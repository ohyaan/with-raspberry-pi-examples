"""HTTP/3-only client vs independent aioquic HTTP/3 peers, loopback only."""
import asyncio
import os
from pathlib import Path
import socket
import sys
import tempfile

from aioquic.asyncio.protocol import QuicConnectionProtocol
from aioquic.asyncio.server import QuicServer
from aioquic.h3.connection import H3Connection
from aioquic.h3.events import HeadersReceived
from aioquic.quic.configuration import QuicConfiguration

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "fixtures"))
from http3_server import BODY, start
from tls_echo import certificates


async def check(case, binaries, directory):
    certificates(directory / "certs")
    certificates(directory / "other")
    certs = directory / "certs"
    server = None
    delayed = []
    processes = []
    requests = []
    violations = []

    async def run(*args, good=False, environment=None):
        process = await asyncio.create_subprocess_exec(
            str(binaries / "http3_client"), *map(str, args),
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE,
            env=environment)
        processes.append(process)
        out, error = await asyncio.wait_for(process.communicate(), 5)
        assert (process.returncode == 0) == good, (case, out, error)
        return out, error

    try:
        if case == "features":
            out, _ = await run("--features", good=True)
            assert b"http3=yes" in out, out
            return
        if case == "invalid":
            for args in ([], ["0", "missing"], ["65536", "missing"], ["bad", "missing"],
                         ["1", "missing", "bad/name"], ["1", "missing", "localhost", "0"],
                         ["1", "missing", "localhost", "10001"]):
                await run(*args)
            return
        if case in ("idle", "unsupported"):
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as silent:
                silent.bind(("127.0.0.1", 0))
                out, error = await run(silent.getsockname()[1], certs / "ca.pem", "localhost", 200)
                if case == "unsupported":
                    assert b"no HTTP/3 support" in error and not out, (out, error)
                    silent.setblocking(False)
                    try:
                        silent.recv(65536)
                    except BlockingIOError:
                        pass
                    else:
                        raise AssertionError("unsupported runtime emitted UDP data")
                else:
                    assert b"Timeout" in error or b"timed out" in error, error
            return
        if case == "fixture":
            transport, server = await start(0, certs)
            port = transport.get_extra_info("sockname")[1]
            out, _ = await run(port, certs / "ca.pem", good=True)
            assert out == b"http_version=3 status=200 body_bytes=13\n" + BODY, out
            return
        if case == "fixture_process":
            process = await asyncio.create_subprocess_exec(
                sys.executable, str(Path(__file__).resolve().parents[1] / "fixtures/http3_server.py"),
                "0", str(certs), "--session-ms", "1200",
                stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
            processes.append(process)
            line = await asyncio.wait_for(process.stdout.readline(), 2)
            assert line.startswith(b"LISTEN "), (line, await process.stderr.read())
            out, _ = await run(int(line.split()[1]), certs / "ca.pem", good=True)
            assert out == b"http_version=3 status=200 body_bytes=13\n" + BODY, out
            output, error = await asyncio.wait_for(process.communicate(), 3)
            assert process.returncode == 0, (output, error)
            return
        if case == "tcp_only":
            accepted = []

            async def tcp_peer(reader, writer):
                accepted.append(True)
                writer.close()
                await writer.wait_closed()

            tcp = await asyncio.start_server(tcp_peer, "127.0.0.1", 0)
            try:
                await run(tcp.sockets[0].getsockname()[1], certs / "ca.pem", "localhost", 200)
                assert not accepted, "HTTP/3-only fell back to TCP"
            finally:
                tcp.close()
                await tcp.wait_closed()
            return

        class Peer(QuicConnectionProtocol):
            def __init__(self, *args, **kwargs):
                super().__init__(*args, **kwargs)
                self.http = H3Connection(self._quic)

            def quic_event_received(self, event):
                for item in self.http.handle_event(event):
                    if not isinstance(item, HeadersReceived):
                        continue
                    requests.append(item)
                    fields = dict(item.headers)
                    if (len(requests) != 1 or not item.stream_ended or
                        fields.get(b":method") != b"GET" or fields.get(b":scheme") != b"https" or
                        fields.get(b":path") != b"/" or
                        fields.get(b":authority") != f"localhost:{port}".encode()):
                        violations.append("wrong request fields/count/end")
                    payload = {"empty": b"", "binary": b"\x00\xff\x01", "body_limit": b"x" * 16384,
                               "oversized_length": b"x" * 16385,
                               "oversized_stream": b"x" * 16385}.get(case, BODY)
                    status = b"404" if case == "not_found" else b"302" if case == "redirect" else b"200"
                    headers = [(b":status", status), (b"content-type", b"text/plain")]
                    if case not in ("no_length", "oversized_stream"):
                        length = len(payload) + (1 if case == "short_body" else 0)
                        headers.append((b"content-length", str(length).encode()))
                    if case == "redirect":
                        headers.append((b"location", b"https://outside.example/"))
                    if case == "huge_headers":
                        headers.append((b"x-padding", b"p" * 17000))
                    self.http.send_headers(item.stream_id, headers)
                    if case == "reset":
                        self._quic.reset_stream(item.stream_id, error_code=0x10C)
                    elif case == "close_early":
                        self.close()
                    elif case in ("split", "trickle"):
                        step = 3 if case == "split" else 1
                        interval = 0.02 if case == "split" else 0.15

                        def piece(index):
                            end = min(index + step, len(payload))
                            self.http.send_data(item.stream_id, payload[index:end], end_stream=end == len(payload))
                            self.transmit()
                            if end < len(payload):
                                delayed.append(asyncio.get_running_loop().call_later(interval, piece, end))

                        piece(0)
                    else:
                        self.http.send_data(item.stream_id, payload, end_stream=case != "no_fin")
                    self.transmit()

        alpns = [] if case == "no_alpn" else ["wpi-echo/1"] if case == "wrong_alpn" else ["h3"]
        config = QuicConfiguration(is_client=False, alpn_protocols=alpns)
        config.load_cert_chain(str(certs / "server.pem"), str(certs / "server.key"))
        transport, server = await asyncio.get_running_loop().create_datagram_endpoint(
            lambda: QuicServer(configuration=config, create_protocol=Peer, retry=True),
            local_addr=("127.0.0.1", 0))
        port = transport.get_extra_info("sockname")[1]
        ca = directory / "other/ca.pem" if case == "wrong_ca" else certs / "ca.pem"
        if case == "missing_ca":
            ca = certs / "missing.pem"
        elif case == "bad_ca":
            ca = directory / "bad.pem"
            ca.write_text("not a certificate")
        environment = dict(os.environ)
        if case == "proxy_environment":
            environment.update(https_proxy="http://127.0.0.1:1", ALL_PROXY="http://127.0.0.1:1")
        good = case in ("normal", "empty", "binary", "body_limit", "no_length", "split", "proxy_environment")
        out, error = await run(port, ca, "wrong.example" if case == "wrong_host" else "localhost",
                               3000 if good else 900, good=good, environment=environment)
        assert not violations, violations
        if case in ("wrong_ca", "wrong_host", "missing_ca", "bad_ca", "no_alpn", "wrong_alpn"):
            assert not requests and not out, (requests, out, error)
        else:
            assert len(requests) == 1, (requests, out, error)
        if good:
            payload = {"empty": b"", "binary": b"\x00\xff\x01", "body_limit": b"x" * 16384}.get(case, BODY)
            assert out == f"http_version=3 status=200 body_bytes={len(payload)}\n".encode() + payload, out[:200]
        if case in ("no_fin", "trickle"):
            assert b"Timeout" in error or b"timed out" in error, error
        if case in ("not_found", "redirect"):
            assert b"expected HTTP/3 status 200" in error, error
    finally:
        for handle in delayed:
            handle.cancel()
        if server:
            server.close()
        for process in processes:
            if process.returncode is None:
                process.terminate()
            await process.communicate()


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="network-http3-") as temporary:
        async def bounded():
            async with asyncio.timeout(9):
                await check(sys.argv[1], Path(sys.argv[2]).resolve(), Path(temporary))

        asyncio.run(bounded())
