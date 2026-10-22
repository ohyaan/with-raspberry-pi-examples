"""Bounded multi-client TCP checks with independent loopback peers."""
import contextlib
import pathlib
import selectors
import socket
import struct
import subprocess
import sys

case, directory = sys.argv[1:]
root = pathlib.Path(directory)


def stop(process):
    if process.poll() is None:
        process.terminate()
    process.communicate(timeout=3)


def run(name, *args):
    return subprocess.run([str(root / name), *args], text=True,
                          capture_output=True, timeout=7)


def start_server(budget='10000'):
    process = subprocess.Popen([str(root / 'tcp_multi_server'), '0', budget],
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


def connect(port):
    return socket.create_connection(('127.0.0.1', port), timeout=5)


def receive_all(peer, allow_reset=False):
    data = bytearray()
    while True:
        try:
            chunk = peer.recv(128)
        except ConnectionResetError:
            if allow_reset:
                return bytes(data)
            raise
        if not chunk:
            return bytes(data)
        data.extend(chunk)
        assert len(data) <= 2048, 'unbounded reply'


def echo(port, payload=b'hello'):
    with connect(port) as peer:
        peer.sendall(payload)
        peer.shutdown(socket.SHUT_WR)
        assert receive_all(peer) == payload


if case in ('invalid_port', 'invalid_budget'):
    if case == 'invalid_port':
        for port in ('', '-1', '65536', '9003x'):
            assert run('tcp_multi_server', port).returncode == 1
            assert run('tcp_multi_client', port).returncode == 1
        assert run('tcp_multi_client', '0').returncode == 1
    else:
        for budget in ('', '0', '299', '30001', '1x'):
            result = run('tcp_multi_server', '0', budget)
            assert result.returncode == 1 and 'run_ms' in result.stderr, result
elif case in ('conflict', 'demo_refused'):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as reserved:
        reserved.bind(('127.0.0.1', 0))
        port = str(reserved.getsockname()[1])
        result = run('tcp_multi_server', port) if case == 'conflict' else run('tcp_multi_client', port)
        assert result.returncode == 1 and ('bind/listen' if case == 'conflict' else 'connect') in result.stderr, result
elif case == 'demo_validation':
    for mode in ('wrong_echo', 'closed_idle'):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
            listener.bind(('127.0.0.1', 0))
            listener.listen(2)
            listener.settimeout(3)
            client = subprocess.Popen([str(root / 'tcp_multi_client'), str(listener.getsockname()[1])],
                                      text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                idle, _ = listener.accept()
                with idle:
                    active, _ = listener.accept()
                    with active:
                        active.settimeout(3)
                        assert receive_all(active) == b'hello'
                        if mode == 'closed_idle':
                            idle.shutdown(socket.SHUT_WR)
                        active.sendall(b'wrong' if mode == 'wrong_echo' else b'hello')
                        active.shutdown(socket.SHUT_WR)
                        out, err = client.communicate(timeout=4)
                        assert client.returncode == 1 and ('echo mismatch' if mode == 'wrong_echo' else 'idle connection') in err, (out, err)
            finally:
                stop(client)
else:
    server, port = start_server('700' if case == 'server_deadline' else '10000')
    try:
        if case == 'demo':
            result = run('tcp_multi_client', str(port))
            assert result.returncode == 0 and result.stdout == (
                'idle connection opened\nactive reply bytes=5 data=hello\nidle connection still open\n'), result
        elif case in ('echo', 'empty'):
            payload = 'x' * 1024 if case == 'echo' else ''
            result = run('tcp_echo_client', str(port), payload)
            assert result.returncode == 0 and result.stdout == f'reply bytes={len(payload)} data={payload}\n', result
        elif case == 'four_clients':
            with contextlib.ExitStack() as stack:
                peers = [stack.enter_context(connect(port)) for _ in range(4)]
                payloads = [bytes([i, 0, 255]) * 300 for i in range(4)]
                for peer, payload in zip(peers, payloads):
                    peer.sendall(payload)
                    peer.shutdown(socket.SHUT_WR)
                for peer, payload in zip(peers, payloads):
                    assert receive_all(peer) == payload
        elif case == 'split':
            with connect(port) as idle, connect(port) as active:
                for byte in b'hello':
                    active.sendall(bytes([byte]))
                    assert active.recv(1) == bytes([byte])
                active.shutdown(socket.SHUT_WR)
                assert receive_all(active) == b''
                idle.setblocking(False)
                try:
                    idle.recv(1)
                    raise AssertionError('idle stream closed early')
                except BlockingIOError:
                    pass
        elif case == 'reset_isolation':
            with connect(port) as bad:
                bad.sendall(b'x')
                assert bad.recv(1) == b'x'
                bad.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
            echo(port)
        elif case == 'oversized':
            with connect(port) as bad:
                bad.sendall(b'x' * 1025)
                bad.shutdown(socket.SHUT_WR)
                assert len(receive_all(bad, allow_reset=True)) <= 1024
            echo(port)
        elif case == 'reuse':
            for i in range(12):
                echo(port, str(i).encode())
        elif case == 'capacity':
            with contextlib.ExitStack() as stack:
                peers = [stack.enter_context(connect(port)) for _ in range(4)]
                # A round-trip proves each of the four slots was admitted.
                for peer in peers:
                    peer.sendall(b'x')
                    assert peer.recv(1) == b'x'
                with connect(port) as excess:
                    assert receive_all(excess, allow_reset=True) == b''
                peers[0].shutdown(socket.SHUT_WR)
                assert receive_all(peers[0]) == b''
                echo(port)
                peers[1].sendall(b'y')
                assert peers[1].recv(1) == b'y'
        elif case == 'peer_deadline':
            with connect(port) as idle:
                assert receive_all(idle) == b''
            echo(port)
        elif case == 'server_deadline':
            with connect(port) as idle:
                out, err = server.communicate(timeout=3)
                assert server.returncode == 0 and out.endswith('stopped\n'), (out, err)
                assert receive_all(idle) == b''
        else:
            raise ValueError(case)
        if case != 'server_deadline':
            assert server.poll() is None, 'peer failure stopped the whole server'
    finally:
        stop(server)
print('PASS multi', case)
