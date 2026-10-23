"""Native peers plus an independent aioquic peer. Loopback only, bounded work."""
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

MESSAGE = b"hello QUIC\n"


async def check(case, binaries, directory):
    certificates(directory / "certs")
    certificates(directory / "other")
    certs = directory / "certs"
    processes = []

    async def launch(name, *arguments):
        process = await asyncio.create_subprocess_exec(
            str(binaries / name), *map(str, arguments),
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
        processes.append(process)
        return process

    async def result(process, good):
        out, error = await asyncio.wait_for(process.communicate(), 4)
        if (process.returncode == 0) != good:
            others = []
            for other in processes:
                if other is not process and other.returncode is not None:
                    others.append(await other.communicate())
            raise AssertionError((case, out, error, others))
        return out, error

    async def native_server(budget=2500):
        process = await launch("quic_echo_server", 0, certs / "server.pem",
                               certs / "server.key", budget)
        line = await asyncio.wait_for(process.stdout.readline(), 2)
        assert line.startswith(b"LISTEN "), (line, await process.stderr.read())
        return process, int(line.split()[1])

    transport = None
    peer_server = None
    relay_transport = None
    delayed = []
    try:
        if case in ("invalid_client", "invalid_server"):
            name = "quic_echo_client" if case.endswith("client") else "quic_echo_server"
            for args in ([], ["bad", "missing", "missing"],
                         ["65536", "missing", "missing"],
                         ["1", "missing", "missing", "0"]):
                await result(await launch(name, *args), False)
            return
        if case in ("missing_key", "key_mismatch"):
            key = certs / "missing.key" if case == "missing_key" else directory / "other/server.key"
            await result(await launch("quic_echo_server", 0, certs / "server.pem", key), False)
            return
        if case == "conflict":
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as occupied:
                occupied.bind(("127.0.0.1", 0))
                await result(await launch("quic_echo_server", occupied.getsockname()[1],
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
                _, error = await result(await launch("quic_echo_client", silent.getsockname()[1],
                                                    certs / "ca.pem", "localhost", 200), False)
                assert b"deadline expired" in error, error
            return
        if case == "pair":
            server, port = await native_server()
            out, _ = await result(await launch("quic_echo_client", port, certs / "ca.pem"), True)
            assert b"verified=localhost alpn=wpi-echo/1" in out, out
            assert b"stream=0 echo_bytes=11 fin=received" in out, out
            out, _ = await result(server, True)
            assert b"stream=0 echo_bytes=11 request_fin=received" in out, out
            return

        if case.startswith("server_") or case == "request_split":
            negative = case in ("server_oversized", "server_no_fin", "server_reset")
            server, port = await native_server(1200 if negative else 3000)
            payload = {"server_empty": b"", "server_binary": b"\x00\xff\x01",
                       "server_limit": b"x" * 64, "server_oversized": b"x" * 65}.get(case, MESSAGE)
            done = asyncio.get_running_loop().create_future()

            class Client(QuicConnectionProtocol):
                data = b""

                def quic_event_received(self, event):
                    if isinstance(event, StreamDataReceived):
                        assert event.stream_id == 0, event
                        self.data += event.data
                        assert len(self.data) <= 65, self.data
                        if event.end_stream and not done.done():
                            done.set_result(self.data)
                    elif isinstance(event, ConnectionTerminated) and not done.done():
                        done.set_exception(ConnectionError("connection ended before FIN"))

            config = QuicConfiguration(is_client=True, alpn_protocols=["wpi-echo/1"],
                                       server_name="localhost")
            config.load_verify_locations(cafile=str(certs / "ca.pem"))
            async with connect("127.0.0.1", port, configuration=config,
                               create_protocol=Client) as peer:
                if case == "server_reset":
                    peer._quic.send_stream_data(0, b"x")
                    peer._quic.reset_stream(0, error_code=9)
                elif case == "request_split":
                    for index, byte in enumerate(payload):
                        peer._quic.send_stream_data(0, bytes([byte]), end_stream=index == len(payload) - 1)
                        peer.transmit()
                        await asyncio.sleep(0.005)
                else:
                    peer._quic.send_stream_data(0, payload, end_stream=case != "server_no_fin")
                peer.transmit()
                if not negative:
                    assert await asyncio.wait_for(done, 2) == payload
                out, error = await result(server, not negative)
                if case == "server_oversized":
                    assert b"exceeds 64 bytes" in error, error
                elif case == "server_no_fin":
                    assert b"deadline expired" in error, error
                elif case != "server_reset":
                    assert f"echo_bytes={len(payload)} request_fin=received".encode() in out, out
                if done.done() and not done.cancelled():
                    done.exception() # Retrieve a deliberate negative-case exception.
                else:
                    done.cancel()
            return

        seen = bytearray()
        response_sent = False

        class Server(QuicConnectionProtocol):
            def quic_event_received(self, event):
                nonlocal response_sent
                if not isinstance(event, StreamDataReceived):
                    return
                assert event.stream_id == 0, event
                seen.extend(event.data)
                assert len(seen) <= 64, seen
                if event.end_stream and not response_sent:
                    response_sent = True
                    assert bytes(seen) == MESSAGE, seen
                    payload = {"wrong_echo": b"wrong", "short_reply": MESSAGE[:-1],
                               "long_reply": b"x" * 65}.get(case, MESSAGE)
                    if case == "reset_reply":
                        self._quic.reset_stream(0, error_code=9)
                    elif case == "close_early":
                        self.close()
                    elif case == "split_reply":
                        self._quic.send_stream_data(0, payload[:3])

                        def rest():
                            self._quic.send_stream_data(0, payload[3:], end_stream=True)
                            self.transmit()

                        delayed.append(asyncio.get_running_loop().call_later(0.03, rest))
                    elif case == "trickle_reply":
                        def piece(index):
                            self._quic.send_stream_data(0, payload[index:index + 1],
                                                        end_stream=index == len(payload) - 1)
                            self.transmit()
                            if index + 1 < len(payload):
                                delayed.append(asyncio.get_running_loop().call_later(0.15, piece, index + 1))

                        piece(0)
                    else:
                        self._quic.send_stream_data(0, payload, end_stream=case != "no_fin")
                    self.transmit()

        alpns = [] if case == "no_alpn" else ["h3"] if case == "wrong_alpn" else ["wpi-echo/1"]
        config = QuicConfiguration(is_client=False, alpn_protocols=alpns)
        config.load_cert_chain(str(certs / "server.pem"), str(certs / "server.key"))
        transport, peer_server = await asyncio.get_running_loop().create_datagram_endpoint(
            lambda: QuicServer(configuration=config, create_protocol=Server, retry=True),
            local_addr=("127.0.0.1", 0))
        port = transport.get_extra_info("sockname")[1]
        dropped = 0
        if case == "drop_initial":
            upstream = ("127.0.0.1", port)

            class Relay(asyncio.DatagramProtocol):
                client = None

                def connection_made(self, relay):
                    self.transport = relay

                def datagram_received(self, data, address):
                    nonlocal dropped
                    if address == upstream:
                        if self.client:
                            self.transport.sendto(data, self.client)
                    else:
                        self.client = address
                        if not dropped:
                            dropped = 1
                        else:
                            self.transport.sendto(data, upstream)

            relay_transport, _ = await asyncio.get_running_loop().create_datagram_endpoint(
                Relay, local_addr=("127.0.0.1", 0))
            port = relay_transport.get_extra_info("sockname")[1]
        ca = directory / "other/ca.pem" if case == "wrong_ca" else certs / "ca.pem"
        if case == "missing_ca":
            ca = certs / "missing.pem"
        if case == "bad_ca":
            ca = directory / "bad.pem"
            ca.write_text("not a certificate")
        hostname = "wrong.example" if case == "wrong_host" else "localhost"
        positive = case in ("peer_server", "split_reply", "drop_initial")
        out, error = await result(await launch("quic_echo_client", port, ca, hostname,
                                               3000 if positive else 1200), positive)
        if case in ("wrong_ca", "wrong_host", "missing_ca", "bad_ca", "no_alpn", "wrong_alpn"):
            assert not seen and b"verified=" not in out, (seen, out, error)
        else:
            assert bytes(seen) == MESSAGE, seen
        if case in ("no_fin", "trickle_reply"):
            assert b"deadline expired" in error, error
        if case == "drop_initial":
            assert dropped == 1, dropped
        if case == "long_reply":
            assert b"exceeds 64 bytes" in error, error
    finally:
        for handle in delayed:
            handle.cancel()
        if relay_transport:
            relay_transport.close()
        if peer_server:
            peer_server.close()
        elif transport:
            transport.close()
        for process in processes:
            if process.returncode is None:
                process.terminate()
            await process.communicate()


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="network-quic-echo-") as temporary:
        async def bounded():
            async with asyncio.timeout(9):
                await check(sys.argv[1], Path(sys.argv[2]).resolve(), Path(temporary))

        asyncio.run(bounded())
