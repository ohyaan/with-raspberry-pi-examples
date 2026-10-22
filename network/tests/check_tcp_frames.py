"""Independent length-prefix wire fixtures for the standalone TCP client."""
import pathlib
import selectors
import socket
import struct
import subprocess
import sys

case, directory = sys.argv[1:]
root = pathlib.Path(directory)


def frame(payload):
    return struct.pack('!I', len(payload)) + payload


def expected(messages):
    return ''.join(f'frame={i + 1} bytes={len(m.encode())} data={m}\n' for i, m in enumerate(messages)) + 'frames=2\n'


def stop(process):
    if process.poll() is None:
        process.terminate()
    process.communicate(timeout=3)


def line_ready(process):
    with selectors.DefaultSelector() as ready:
        ready.register(process.stdout, selectors.EVENT_READ)
        assert ready.select(2), 'client did not emit a frame while connection remained open'
    return process.stdout.readline()


def read_request(peer):
    request = bytearray()
    while True:
        chunk = peer.recv(128)
        if not chunk:
            return bytes(request)
        request.extend(chunk)
        assert len(request) <= 136


if case in ('echo', 'empty', 'limit', 'utf8'):
    messages = {'echo': ('hello', 'Pi'), 'empty': ('', ''), 'limit': ('a' * 64, 'b' * 64), 'utf8': ('Pi π', '温度')}[case]
    server = subprocess.Popen([str(root / 'tcp_echo_server'), '0'], text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(server.stdout, selectors.EVENT_READ)
            assert ready.select(3), 'server not ready'
        line = server.stdout.readline().strip()
        assert line.startswith('listening 127.0.0.1:'), line
        port = line.rsplit(':', 1)[1]
        result = subprocess.run([str(root / 'tcp_frames_client'), port, *messages],
                                text=True, capture_output=True, timeout=8)
        assert result.returncode == 0 and result.stdout == expected(messages), (result.stdout, result.stderr)
        out, err = server.communicate(timeout=3)
        size = sum(4 + len(m.encode()) for m in messages)
        assert server.returncode == 0 and out == f'accepted connection\nechoed bytes={size}\n', (out, err)
    finally:
        stop(server)
elif case == 'invalid':
    for args in (['-1'], ['0'], ['65536'], ['9001x'], ['9001', 'one'], ['9001', 'x' * 65, 'Pi']):
        result = subprocess.run([str(root / 'tcp_frames_client'), *args], text=True, capture_output=True, timeout=3)
        assert result.returncode != 0, args
else:
    first, second = b'hello', b'Pi'
    good = frame(first) + frame(second)
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(3)
        client = subprocess.Popen([str(root / 'tcp_frames_client'), str(listener.getsockname()[1])],
                                  text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        consumed = ''
        try:
            peer, _ = listener.accept()
            with peer:
                peer.settimeout(3)
                assert read_request(peer) == good
                error = None
                if case == 'combined':
                    peer.sendall(good)
                elif case == 'split':
                    for byte in good:
                        peer.sendall(bytes([byte]))
                elif case == 'open_frames':
                    peer.sendall(frame(first))
                    consumed += line_ready(client)
                    assert consumed == 'frame=1 bytes=5 data=hello\n'
                    peer.sendall(frame(second))
                    consumed += line_ready(client)
                    assert consumed.endswith('frame=2 bytes=2 data=Pi\n')
                elif case == 'truncated_header':
                    peer.sendall(b'\x00\x00')
                    error = 'truncated header'
                elif case == 'truncated_payload':
                    peer.sendall(struct.pack('!I', 5) + b'he')
                    error = 'truncated payload'
                elif case == 'oversized_header':
                    peer.sendall(struct.pack('!I', 65))
                    error = 'frame exceeds 64 bytes'
                elif case == 'missing_frame':
                    peer.sendall(frame(first))
                    error = 'missing frames'
                elif case == 'extra_frame':
                    peer.sendall(good + frame(b'extra'))
                    error = 'unexpected frame'
                elif case == 'mismatch':
                    peer.sendall(frame(b'wrong'))
                    error = 'frame echo mismatch'
                elif case == 'idle':
                    error = 'recv'
                else:
                    raise ValueError(case)
                # Oversized length must be rejected before waiting for EOF.
                if case not in ('oversized_header', 'idle'):
                    peer.shutdown(socket.SHUT_WR)
                out, err = client.communicate(timeout=2 if case == 'oversized_header' else 6)
                if error:
                    assert client.returncode != 0 and error in err, (out, err)
                else:
                    assert client.returncode == 0 and consumed + out == expected(('hello', 'Pi')), (consumed, out, err)
        finally:
            stop(client)
print('PASS frames', case)
