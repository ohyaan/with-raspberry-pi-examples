"""Two-stream ordering oracle using an independent, pinned aioquic peer."""
import asyncio
from pathlib import Path
import socket
import sys
import tempfile

from aioquic.asyncio import connect
from aioquic.asyncio.protocol import QuicConnectionProtocol
from aioquic.asyncio.server import QuicServer
from aioquic.quic.configuration import QuicConfiguration
from aioquic.quic.events import ConnectionTerminated, StreamDataReceived

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "fixtures"))
from tls_echo import certificates

ALPN = "wpi-streams/1"
MESSAGES = {0: b"slow\n", 4: b"fast\n"}


async def check(case, binaries, directory):
    certificates(directory / "certs")
    certificates(directory / "other")
    certs = directory / "certs"
    processes = []
    transport = None
    peer_server = None
    delayed = []
    futures = []

    async def launch(name, *args):
        process = await asyncio.create_subprocess_exec(
            str(binaries / name), *map(str, args), stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE)
        processes.append(process)
        return process

    async def result(process, good):
        out, error = await asyncio.wait_for(process.communicate(), 5)
        if (process.returncode == 0) != good:
            others = [await other.communicate() for other in processes
                      if other is not process and other.returncode is not None]
            raise AssertionError((case, out, error, others))
        return out, error

    async def native_server(budget=3500):
        process = await launch("quic_streams_server", 0, certs / "server.pem",
                               certs / "server.key", budget)
        line = await asyncio.wait_for(process.stdout.readline(), 2)
        assert line.startswith(b"LISTEN "), (line, await process.stderr.read())
        return process, int(line.split()[1])

    try:
        if case in ("invalid_client", "invalid_server"):
            name = "quic_streams_client" if case.endswith("client") else "quic_streams_server"
            for args in ([], ["bad", "missing", "missing"],
                         ["65536", "missing", "missing"],
                         ["1", "missing", "missing", "0"]):
                await result(await launch(name, *args), False)
            return
        if case in ("missing_key", "key_mismatch"):
            key = certs / "missing.key" if case == "missing_key" else directory / "other/server.key"
            await result(await launch("quic_streams_server", 0, certs / "server.pem", key), False)
            return
        if case == "conflict":
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as occupied:
                occupied.bind(("127.0.0.1", 0))
                await result(await launch("quic_streams_server", occupied.getsockname()[1],
                                          certs / "server.pem", certs / "server.key"), False)
            return
        if case == "server_idle":
            server, _ = await native_server(200)
            _, error = await result(server, False)
            assert b"deadline expired" in error, error
            return
        if case == "client_idle":
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as silent:
                silent.bind(("127.0.0.1", 0))
                _, error = await result(await launch("quic_streams_client", silent.getsockname()[1],
                                                    certs / "ca.pem", "localhost", 200), False)
                assert b"deadline expired" in error, error
            return
        if case == "pair":
            server, port = await native_server()
            out, _ = await result(await launch("quic_streams_client", port, certs / "ca.pem"), True)
            assert out.splitlines() == [b"verified=localhost alpn=wpi-streams/1",
                                        b"COMPLETE stream=4 slow_request_fin=not_sent",
                                        b"COMPLETE stream=0", b"connection_shutdown=complete"], out
            out, _ = await result(server, True)
            assert out.splitlines() == [b"FIN_QUEUED stream=4 bytes=5",
                                        b"FIN_QUEUED stream=0 bytes=5",
                                        b"connection_shutdown=complete"], out
            return

        if case.startswith("native_"):
            negative = case in ("native_oversized_a", "native_oversized_b", "native_no_a_fin",
                                "native_no_b_fin", "native_reset_a", "native_reset_b",
                                "native_extra_stream", "native_uni")
            server, port = await native_server(1200 if negative else 3500)
            done = {i: asyncio.get_running_loop().create_future() for i in (0, 4)}
            futures.extend(done.values())

            class Client(QuicConnectionProtocol):
                def __init__(self, *args, **kwargs):
                    super().__init__(*args, **kwargs)
                    self.data = {0: bytearray(), 4: bytearray()}

                def quic_event_received(self, event):
                    if isinstance(event, StreamDataReceived):
                        if event.stream_id not in done:
                            return
                        self.data[event.stream_id].extend(event.data)
                        assert len(self.data[event.stream_id]) <= 65
                        if event.end_stream and not done[event.stream_id].done():
                            done[event.stream_id].set_result(bytes(self.data[event.stream_id]))
                    elif isinstance(event, ConnectionTerminated):
                        for future in done.values():
                            if not future.done():
                                future.set_exception(ConnectionError("connection ended before FIN"))

            config = QuicConfiguration(is_client=True, alpn_protocols=[ALPN], server_name="localhost")
            config.load_verify_locations(cafile=str(certs / "ca.pem"))
            payloads = dict(MESSAGES)
            if case == "native_empty":
                payloads = {0: b"", 4: b""}
            elif case == "native_binary":
                payloads = {0: b"\x00\xff", 4: b"\x81\x00\xfe"}
            elif case == "native_limit":
                payloads = {0: b"a" * 64, 4: b"b" * 64}
            elif case.startswith("native_oversized_"):
                payloads[0 if case.endswith("_a") else 4] = b"x" * 65
            async with connect("127.0.0.1", port, configuration=config,
                               create_protocol=Client) as peer:
                if case == "native_uni":
                    peer._quic.send_stream_data(2, b"x", end_stream=True)
                if case != "native_b_before_a":
                    peer._quic.send_stream_data(0, payloads[0])
                if case == "native_reset_a":
                    peer._quic.reset_stream(0, error_code=9)
                if case == "native_split":
                    for index, byte in enumerate(payloads[4]):
                        peer._quic.send_stream_data(4, bytes([byte]),
                                                    end_stream=index == len(payloads[4]) - 1)
                        peer.transmit()
                        await asyncio.sleep(0.01)
                else:
                    peer._quic.send_stream_data(4, payloads[4],
                                                end_stream=case not in ("native_no_b_fin", "native_reset_b"))
                if case == "native_reset_b":
                    peer._quic.reset_stream(4, error_code=9)
                if case == "native_extra_stream":
                    peer._quic.send_stream_data(8, b"extra", end_stream=True)
                peer.transmit()
                if not negative or case == "native_no_a_fin":
                    assert await asyncio.wait_for(done[4], 2) == payloads[4]
                    assert not done[0].done(), "stream 0 must await its own request FIN"
                if not negative:
                    if case == "native_b_before_a":
                        peer._quic.send_stream_data(0, payloads[0], end_stream=True)
                    else:
                        peer._quic.send_stream_data(0, b"", end_stream=True)
                    peer.transmit()
                    assert await asyncio.wait_for(done[0], 2) == payloads[0]
                out, error = await result(server, not negative)
                if "oversized" in case:
                    assert b"exceeds 64 bytes" in error, error
                if case in ("native_no_a_fin", "native_no_b_fin"):
                    assert b"deadline expired" in error, error
                if case in ("native_extra_stream", "native_uni"):
                    assert b"only client bidirectional streams 0 and 4" in error, error
                if case == "native_no_a_fin":
                    assert b"FIN_QUEUED stream=4" in out and b"FIN_QUEUED stream=0" not in out, out
                elif not negative:
                    lines = out.splitlines()
                    assert lines == [f"FIN_QUEUED stream=4 bytes={len(payloads[4])}".encode(),
                                     f"FIN_QUEUED stream=0 bytes={len(payloads[0])}".encode(),
                                     b"connection_shutdown=complete"], out
            return

        seen = {0: bytearray(), 4: bytearray()}
        ended = set()
        violations = []
        fast_reply_started = False

        class Server(QuicConnectionProtocol):
            def quic_event_received(self, event):
                nonlocal fast_reply_started
                if not isinstance(event, StreamDataReceived):
                    return
                sid = event.stream_id
                if sid not in seen:
                    violations.append("unexpected request stream")
                    self.close()
                    return
                seen[sid].extend(event.data)
                if len(seen[sid]) > 5:
                    violations.append("request exceeds five expected bytes")
                    self.close()
                    return
                if not event.end_stream:
                    return
                ended.add(sid)
                if bytes(seen[sid]) != MESSAGES[sid]:
                    violations.append("wrong request bytes")
                if sid == 0 and not fast_reply_started:
                    violations.append("slow request ended before fast response was sent")
                if sid == 4:
                    fast_reply_started = True
                if case == "early_close":
                    self.close()
                    return
                if case == ("reset_a" if sid == 0 else "reset_b"):
                    self._quic.reset_stream(sid, error_code=9)
                else:
                    payload = MESSAGES[sid]
                    if case == ("wrong_a" if sid == 0 else "wrong_b"):
                        payload = b"wrong"
                    if case == ("oversized_a" if sid == 0 else "oversized_b"):
                        payload = b"x" * 65
                    fin = case != ("no_a_fin" if sid == 0 else "no_b_fin")
                    if case == "split_reply":
                        self._quic.send_stream_data(sid, payload[:2])

                        def rest():
                            self._quic.send_stream_data(sid, payload[2:], end_stream=fin)
                            self.transmit()

                        delayed.append(asyncio.get_running_loop().call_later(0.02, rest))
                    else:
                        self._quic.send_stream_data(sid, payload, end_stream=fin)
                self.transmit()

        alpns = [] if case == "no_alpn" else ["wpi-echo/1"] if case == "wrong_alpn" else [ALPN]
        config = QuicConfiguration(is_client=False, alpn_protocols=alpns)
        config.load_cert_chain(str(certs / "server.pem"), str(certs / "server.key"))
        transport, peer_server = await asyncio.get_running_loop().create_datagram_endpoint(
            lambda: QuicServer(configuration=config, create_protocol=Server, retry=True),
            local_addr=("127.0.0.1", 0))
        port = transport.get_extra_info("sockname")[1]
        ca = directory / "other/ca.pem" if case == "wrong_ca" else certs / "ca.pem"
        if case == "missing_ca":
            ca = certs / "missing.pem"
        elif case == "bad_ca":
            ca = directory / "bad.pem"
            ca.write_text("not a certificate")
        hostname = "wrong.example" if case == "wrong_host" else "localhost"
        positive = case in ("peer_server", "split_reply")
        out, error = await result(await launch("quic_streams_client", port, ca, hostname,
                                               3500 if positive else 1200), positive)
        assert not violations, violations
        if case in ("no_alpn", "wrong_alpn", "wrong_ca", "wrong_host", "missing_ca", "bad_ca"):
            assert not any(seen.values()) and b"verified=" not in out, (seen, out)
        else:
            assert bytes(seen[4]) == MESSAGES[4] and 4 in ended, (seen, ended)
        if positive:
            assert ended == {0, 4} and bytes(seen[0]) == MESSAGES[0], (seen, ended)
            assert out.splitlines()[1:3] == [b"COMPLETE stream=4 slow_request_fin=not_sent",
                                             b"COMPLETE stream=0"], out
        if case in ("no_a_fin", "no_b_fin"):
            assert b"deadline expired" in error, error
        if case.endswith("_b") or case == "early_close":
            assert 0 not in ended, ended
        if case in ("no_a_fin", "wrong_a", "reset_a", "oversized_a"):
            assert 0 in ended and b"COMPLETE stream=4" in out and b"COMPLETE stream=0" not in out, out
        if case in ("oversized_a", "oversized_b"):
            assert b"exceeds 64 bytes" in error, error
    finally:
        for handle in delayed:
            handle.cancel()
        if peer_server:
            peer_server.close()
        elif transport:
            transport.close()
        for process in processes:
            if process.returncode is None:
                process.terminate()
            await process.communicate()
        for future in futures:
            if future.done() and not future.cancelled():
                future.exception()
            else:
                future.cancel()


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="network-quic-streams-") as temporary:
        async def bounded():
            async with asyncio.timeout(10):
                await check(sys.argv[1], Path(sys.argv[2]).resolve(), Path(temporary))

        asyncio.run(bounded())
