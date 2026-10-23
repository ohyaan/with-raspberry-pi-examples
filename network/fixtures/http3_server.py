"""Bounded loopback HTTP/3 fixture using aioquic, not Python's ssl module."""
import argparse
import asyncio
from pathlib import Path

from aioquic.asyncio.protocol import QuicConnectionProtocol
from aioquic.asyncio.server import QuicServer
from aioquic.h3.connection import H3Connection
from aioquic.h3.events import HeadersReceived
from aioquic.quic.configuration import QuicConfiguration
from aioquic.quic.events import ConnectionTerminated

BODY = b"hello HTTP/3\n"


class Http3Server(QuicConnectionProtocol):
    def __init__(self, *args, finished=None, **kwargs):
        super().__init__(*args, **kwargs)
        self.http = H3Connection(self._quic)
        self.finished = finished
        self.requests = 0

    def quic_event_received(self, event):
        if isinstance(event, ConnectionTerminated) and self.finished and not self.finished.done():
            self.finished.set_result(None)
        for item in self.http.handle_event(event):
            if isinstance(item, HeadersReceived):
                self.requests += 1
                fields = dict(item.headers)
                if (self.requests != 1 or not item.stream_ended or
                    fields.get(b":method") != b"GET" or
                    fields.get(b":scheme") != b"https" or fields.get(b":path") != b"/"):
                    self.close(error_code=0x10B, reason_phrase="one GET / expected")
                    return
                self.http.send_headers(item.stream_id, [(b":status", b"200"),
                    (b"content-type", b"text/plain"), (b"content-length", str(len(BODY)).encode())])
                self.http.send_data(item.stream_id, BODY, end_stream=True)
                self.transmit()


async def start(port, directory, protocol=Http3Server):
    config = QuicConfiguration(is_client=False, alpn_protocols=["h3"])
    config.load_cert_chain(str(directory / "server.pem"), str(directory / "server.key"))
    return await asyncio.get_running_loop().create_datagram_endpoint(
        lambda: QuicServer(configuration=config, create_protocol=protocol, retry=True),
        local_addr=("127.0.0.1", port))


async def serve(port, directory, session_ms=15000):
    finished = asyncio.get_running_loop().create_future()

    def protocol(*args, **kwargs):
        return Http3Server(*args, finished=finished, **kwargs)

    transport, server = await start(port, directory, protocol)
    print(f"LISTEN {transport.get_extra_info('sockname')[1]}", flush=True)
    try:
        await asyncio.wait_for(finished, session_ms / 1000)
    except TimeoutError:
        # A completed HTTP response need not be followed by an observed peer
        # connection-close notification. End the bounded fixture normally.
        print("WAIT_EXPIRED", flush=True)
    finally:
        server.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", type=int, choices=range(65536), metavar="port")
    parser.add_argument("directory", type=Path)
    parser.add_argument("--session-ms", type=int, default=15000,
                        choices=range(100, 15001), metavar="100..15000")
    args = parser.parse_args()
    asyncio.run(serve(args.port, args.directory.resolve(), args.session_ms))
