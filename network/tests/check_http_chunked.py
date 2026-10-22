"""Independent wire fixtures for the restricted chunked response decoder."""
import pathlib
import selectors
import socket
import subprocess
import sys
import time

case, directory = sys.argv[1:]
root = pathlib.Path(directory)
fixture = pathlib.Path(__file__).resolve().parents[1] / 'fixtures/http_chunked_server.py'


def stop(process):
    if process.poll() is None:
        process.terminate()
    process.communicate(timeout=3)


if case in ('fixture', 'fixture_missing'):
    server = subprocess.Popen([sys.executable, str(fixture), '0'], text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(server.stdout, selectors.EVENT_READ)
            assert ready.select(3)
        port = server.stdout.readline().strip().rsplit(':', 1)[1]
        path = '/hello.txt' if case == 'fixture' else '/missing'
        result = subprocess.run([str(root / 'http_chunked_client'), port, path], capture_output=True, timeout=5)
        body = b'Hello from Pi.\n' if case == 'fixture' else b'Not found\n'
        code = 200 if case == 'fixture' else 404
        assert result.returncode == 0 and result.stdout == f'status={code} bytes={len(body)}\n'.encode() + body, result
        out, err = server.communicate(timeout=3)
        assert server.returncode == 0, (out, err)
    finally:
        stop(server)
elif case in ('invalid', 'refused'):
    if case == 'invalid':
        for args in ([], ['0'], ['65536'], ['8082', '/has space'], ['8082', '/bad\r\nHost: x'], ['8082', '/', '0']):
            result = subprocess.run([str(root / 'http_chunked_client'), *args], capture_output=True, timeout=3)
            assert result.returncode == 1, args
    else:
        with socket.socket() as reserved:
            reserved.bind(('127.0.0.1', 0))
            result = subprocess.run([str(root / 'http_chunked_client'), str(reserved.getsockname()[1])], capture_output=True, timeout=3)
            assert result.returncode == 1 and b'connect' in result.stderr, result
else:
    header = b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n'
    wire = b'2\r\nhe\r\n3\r\nllo\r\n0\r\n\r\n'
    fixtures = {
        'combined': (header + wire, None, b'hello'),
        'split': (header + wire, None, b'hello'),
        'empty': (header + b'0\r\n\r\n', None, b''),
        'binary': (header + b'2\r\n\x00\xff\r\n3\r\nA\r\n\r\n0\r\n\r\n', None, b'\x00\xffA\r\n'),
        'hex_case': (header + b'A\r\n0123456789\r\n0\r\n\r\n', None, b'0123456789'),
        'mixed_case': (b'HTTP/1.1 200 OK\r\ntRaNsFeR-EnCoDiNg:\tChUnKeD \r\n\r\n' + wire, None, b'hello'),
        'truncated_size': (header + b'5\r', b'truncated chunked body', None),
        'truncated_data': (header + b'5\r\nhe', b'truncated chunked body', None),
        'truncated_final': (header + b'0\r\n', b'truncated chunked body', None),
        'bad_size': (header + b'z\r\n', b'invalid chunk size', None),
        'size_overflow': (header + b'10000000000000000\r\n', b'chunk size', None),
        'large_chunk': (header + b'1001\r\n', b'decoded body exceeds', None),
        'aggregate_limit': (header + b'1000\r\n' + b'x' * 4096 + b'\r\n1\r\n', b'decoded body exceeds', None),
        'bad_crlf': (header + b'2\r\nheXX', b'missing chunk data CRLF', None),
        'extensions': (header + b'2;x=1\r\nhe\r\n0\r\n\r\n', b'unsupported extension', None),
        'trailers': (header + b'0\r\nX-Result: yes\r\n\r\n', b'trailers unsupported', None),
        'missing_te': (b'HTTP/1.1 200 OK\r\nX-Test: yes\r\n\r\n' + wire, b'missing chunked', None),
        'duplicate_te': (b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nTransfer-Encoding: chunked\r\n\r\n', b'expected one chunked', None),
        'cl_and_te': (b'HTTP/1.1 200 OK\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n' + wire, b'Content-Length forbidden', None),
        'coding_chain': (b'HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip, chunked\r\n\r\n', b'expected one chunked', None),
        'content_encoding': (b'HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nTransfer-Encoding: chunked\r\n\r\n', b'response feature', None),
        'bad_version': (b'HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n' + wire, b'invalid status line', None),
        'wire_limit': (header + b'1\r\nx\r\n' * 3000 + b'0\r\n\r\n', b'wire response exceeds', None),
        'headers_limit': (b'HTTP/1.1 200 OK\r\nX-Pad: ' + b'x' * 4096, b'headers exceed', None),
        'idle': (b'', b'receive deadline', None),
        'trickle': (b'', b'receive deadline', None),
        'surplus': (header + wire + b'not another response', None, b'hello'),
        'body_limit': (header + b'1000\r\n' + b'x' * 4096 + b'\r\n0\r\n\r\n', None, b'x' * 4096),
        'long_size_split': (header + b'0000000000000001\r', None, b'x'),
    }
    response, error, body = fixtures[case]
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(3)
        port = str(listener.getsockname()[1])
        client = subprocess.Popen([str(root / 'http_chunked_client'), port, '/hello.txt', '500'],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            peer, _ = listener.accept()
            with peer:
                peer.settimeout(3)
                request = bytearray()
                while not request.endswith(b'\r\n\r\n'):
                    chunk = peer.recv(128)
                    assert chunk
                    request.extend(chunk)
                    assert len(request) <= 512
                assert request == f'GET /hello.txt HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nConnection: close\r\n\r\n'.encode()
                if case == 'trickle':
                    end = time.monotonic() + 1.8
                    while client.poll() is None and time.monotonic() < end:
                        try:
                            peer.sendall(b'x')
                        except (BrokenPipeError, ConnectionResetError):
                            break
                        time.sleep(0.05)
                    assert client.poll() is not None, 'progress renewed the budget'
                else:
                    try:
                        if case == 'long_size_split':
                            peer.sendall(response)
                            time.sleep(0.05)
                            peer.sendall(b'\nx\r\n0\r\n\r\n')
                        elif case == 'split':
                            for byte in response:
                                peer.sendall(bytes([byte]))
                        else:
                            peer.sendall(response)
                    except (BrokenPipeError, ConnectionResetError):
                        assert error or case == 'surplus'
                if case.startswith('truncated_'):
                    peer.shutdown(socket.SHUT_WR)
                out, err = client.communicate(timeout=3)
                if error:
                    assert client.returncode == 1 and error in err, (out, err)
                else:
                    assert client.returncode == 0 and out == f'status=200 bytes={len(body)}\n'.encode() + body, (out, err)
        finally:
            stop(client)
print('PASS HTTP chunked', case)
