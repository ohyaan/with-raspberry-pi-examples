"""Finite TCP checks on temporary IPv4 loopback ports."""
import pathlib
import selectors
import socket
import subprocess
import sys

case, directory = sys.argv[1:]
root = pathlib.Path(directory)


def run(name, *args):
    return subprocess.run([str(root / name), *args], text=True, capture_output=True, timeout=8)


def stop(process):
    if process.poll() is None:
        process.terminate()
    process.communicate(timeout=3)


def start_server():
    process = subprocess.Popen([str(root / 'tcp_echo_server'), '0'],
                               text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(process.stdout, selectors.EVENT_READ)
            assert ready.select(3), 'server not ready'
        line = process.stdout.readline().strip()
        assert line.startswith('listening 127.0.0.1:'), line
        return process, int(line.rsplit(':', 1)[1])
    except BaseException:
        stop(process)
        raise


def completed_server(process, size):
    out, err = process.communicate(timeout=6)
    assert process.returncode == 0, (out, err)
    assert out == f'accepted connection\nechoed bytes={size}\n', out


def receive_all(peer):
    result = bytearray()
    while True:
        chunk = peer.recv(128)
        if not chunk:
            return bytes(result)
        result.extend(chunk)
        assert len(result) <= 2048, 'unbounded reply'


if case in ('echo', 'empty', 'limit'):
    payload = {'echo': 'hello', 'empty': '', 'limit': 'x' * 1024}[case]
    process, port = start_server()
    try:
        client = run('tcp_echo_client', str(port), payload)
        assert client.returncode == 0, client.stderr
        assert client.stdout == f'reply bytes={len(payload)} data={payload}\n'
        completed_server(process, len(payload))
    finally:
        stop(process)
elif case in ('binary', 'split', 'oversized', 'idle_server'):
    process, port = start_server()
    try:
        with socket.create_connection(('127.0.0.1', port), timeout=3) as peer:
            if case == 'idle_server':
                out, err = process.communicate(timeout=6)
                assert process.returncode != 0 and 'recv' in err, (out, err)
            elif case == 'oversized':
                peer.sendall(b'x' * 1025)
                peer.shutdown(socket.SHUT_WR)
                out, err = process.communicate(timeout=6)
                assert process.returncode != 0 and 'request exceeds 1024' in err, (out, err)
            elif case == 'split':
                for byte in b'hello':
                    peer.sendall(bytes([byte]))
                    # Wait for this echo before sending the next byte. The
                    # server must process an incomplete request incrementally.
                    assert peer.recv(1) == bytes([byte])
                peer.shutdown(socket.SHUT_WR)
                assert receive_all(peer) == b''
                completed_server(process, 5)
            else:
                payload = b'\x00\xffA\x00B'
                peer.sendall(payload)
                peer.shutdown(socket.SHUT_WR)
                assert receive_all(peer) == payload
                completed_server(process, len(payload))
    finally:
        stop(process)
    if case == 'oversized':
        client = run('tcp_echo_client', '9001', 'x' * 1025)
        assert client.returncode != 0 and 'exceeds 1024' in client.stderr
elif case in ('client_split', 'early_eof', 'idle_client'):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(3)
        process = subprocess.Popen([str(root / 'tcp_echo_client'), str(listener.getsockname()[1]), 'hello'],
                                   text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            peer, _ = listener.accept()
            with peer:
                peer.settimeout(3)
                assert receive_all(peer) == b'hello'
                if case == 'client_split':
                    for chunk in (b'h', b'el', b'lo'):
                        peer.sendall(chunk)
                    peer.shutdown(socket.SHUT_WR)
                elif case == 'early_eof':
                    peer.sendall(b'he')
                    peer.shutdown(socket.SHUT_WR)
                out, err = process.communicate(timeout=6)
                if case == 'client_split':
                    assert process.returncode == 0 and out == 'reply bytes=5 data=hello\n', (out, err)
                else:
                    expected = 'echo mismatch' if case == 'early_eof' else 'recv'
                    assert process.returncode != 0 and expected in err, (out, err)
        finally:
            stop(process)
elif case == 'refused':
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as reserved:
        reserved.bind(('127.0.0.1', 0))
        client = run('tcp_echo_client', str(reserved.getsockname()[1]))
        assert client.returncode != 0 and 'connect' in client.stderr
elif case == 'invalid_port':
    for text in ('', '-1', '65536', '9001x', '99999999999999999999999'):
        for name in ('tcp_echo_client', 'tcp_echo_server'):
            result = run(name, text)
            assert result.returncode != 0 and 'port must be' in result.stderr
    assert run('tcp_echo_client', '0').returncode != 0
else:
    raise ValueError(case)
print('PASS tcp', case)
