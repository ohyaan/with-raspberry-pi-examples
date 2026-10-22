"""Finite Linux TCP lifecycle checks, including a non-renewing receive budget."""
import pathlib
import selectors
import socket
import struct
import subprocess
import sys
import time

case, directory = sys.argv[1:]
root = pathlib.Path(directory)


def stop(process):
    if process.poll() is None:
        process.terminate()
    process.communicate(timeout=3)


def run(name, *args):
    return subprocess.run([str(root / name), *args], text=True,
                          capture_output=True, timeout=5)


def start_server(mode):
    process = subprocess.Popen([str(root / 'tcp_lifecycle_server'), '0', mode],
                               text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(process.stdout, selectors.EVENT_READ)
            assert ready.select(3), 'server not ready'
        line = process.stdout.readline().strip()
        assert line.startswith('listening 127.0.0.1:'), line
        return process, line.rsplit(':', 1)[1]
    except BaseException:
        stop(process)
        raise


if case in ('eof', 'reset', 'idle', 'drip'):
    server, port = start_server(case)
    try:
        begin = time.monotonic()
        client = run('tcp_lifecycle_client', port)
        elapsed = time.monotonic() - begin
        if case == 'eof':
            assert client.returncode == 0 and client.stdout == 'eof bytes=5 data=hello\n', client
        elif case == 'reset':
            assert client.returncode == 2 and client.stdout == 'reset bytes=0 data=\n', client
        else:
            assert client.returncode == 3 and client.stdout.startswith('deadline bytes='), client
            # Broad timing bounds tolerate scheduler latency, not a renewed budget.
            assert 0.8 <= elapsed < 2.5, elapsed
            if case == 'idle':
                assert client.stdout == 'deadline bytes=0 data=\n', client
            else:
                data = client.stdout.strip().split('data=', 1)[1]
                assert data and 'abc'.startswith(data), client.stdout
        assert not client.stderr, client.stderr
        out, err = server.communicate(timeout=4)
        assert server.returncode == 0 and out == f'finished mode={case}\n', (out, err)
    finally:
        stop(server)
    if case == 'drip':
        # The article's longer-budget exercise must reach the same peer's EOF.
        server, port = start_server('drip')
        try:
            client = run('tcp_lifecycle_client', port, '3000')
            assert client.returncode == 0 and client.stdout == 'eof bytes=3 data=abc\n', client
            out, err = server.communicate(timeout=3)
            assert server.returncode == 0 and out == 'finished mode=drip\n', (out, err)
        finally:
            stop(server)
elif case in ('empty_eof', 'drained_eof', 'partial_reset', 'trickle_deadline', 'oversized'):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(3)
        process = subprocess.Popen([str(root / 'tcp_lifecycle_client'),
                                    str(listener.getsockname()[1]), '500'],
                                   text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            peer, _ = listener.accept()
            with peer:
                peer.settimeout(3)
                assert peer.recv(1) == b'?'
                if case == 'partial_reset':
                    # Allow time for reads, but do not assume bytes survive RST.
                    peer.sendall(b'he')
                    end = time.monotonic() + 0.1
                    while time.monotonic() < end:
                        time.sleep(0.01)
                    peer.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
                    peer.close()
                elif case == 'trickle_deadline':
                    end = time.monotonic() + 1.8
                    while process.poll() is None and time.monotonic() < end:
                        try:
                            peer.sendall(b'x')
                        except (BrokenPipeError, ConnectionResetError):
                            break
                        time.sleep(0.05)
                    assert process.poll() is not None, 'progress incorrectly renewed the deadline'
                elif case == 'oversized':
                    peer.sendall(b'x' * 1025)
                else:
                    if case == 'drained_eof':
                        peer.sendall(b'x' * 1024)
                    peer.shutdown(socket.SHUT_WR)
                out, err = process.communicate(timeout=3)
            if case == 'empty_eof':
                assert process.returncode == 0 and out == 'eof bytes=0 data=\n', (out, err)
            elif case == 'drained_eof':
                assert process.returncode == 0 and out == 'eof bytes=1024 data=' + 'x' * 1024 + '\n', (out, err)
            elif case == 'partial_reset':
                # RST can discard unread bytes. Do not promise delivery before reset.
                assert process.returncode == 2 and out.startswith('reset bytes='), (out, err)
                assert out.strip().split('data=', 1)[1] in ('', 'h', 'he'), out
            elif case == 'trickle_deadline':
                assert process.returncode == 3 and out.startswith('deadline bytes='), (out, err)
                assert 'data=x' in out, out
            else:
                assert process.returncode == 1 and 'reply exceeds 1024 bytes' in err, (out, err)
        finally:
            stop(process)
elif case in ('invalid_port', 'invalid_budget', 'bad_mode'):
    if case == 'invalid_port':
        for port in ('', '-1', '65536', '9002x'):
            assert run('tcp_lifecycle_client', port).returncode == 1
            assert run('tcp_lifecycle_server', port, 'eof').returncode == 1
        assert run('tcp_lifecycle_client', '0').returncode == 1
    elif case == 'invalid_budget':
        for budget in ('', '0', '-1', '10001', '1x'):
            result = run('tcp_lifecycle_client', '9002', budget)
            assert result.returncode == 1 and 'receive_ms' in result.stderr, result
    else:
        assert run('tcp_lifecycle_server', '0', 'unknown').returncode == 1
elif case in ('refused', 'conflict'):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as reserved:
        reserved.bind(('127.0.0.1', 0))
        port = str(reserved.getsockname()[1])
        result = run('tcp_lifecycle_client', port) if case == 'refused' else run('tcp_lifecycle_server', port, 'eof')
        assert result.returncode == 1 and ('connect' if case == 'refused' else 'bind/listen') in result.stderr, result
elif case in ('bad_request', 'idle_request'):
    server, port = start_server('eof')
    try:
        with socket.create_connection(('127.0.0.1', int(port)), timeout=3) as peer:
            if case == 'bad_request':
                peer.sendall(b'!')
            out, err = server.communicate(timeout=5)
            assert server.returncode == 1 and ('request byte' if case == 'bad_request' else 'recv request') in err, (out, err)
    finally:
        stop(server)
else:
    raise ValueError(case)
print('PASS lifecycle', case)
