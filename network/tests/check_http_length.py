"""Independent bounded Content-Length fixtures, not a general HTTP test suite."""
import pathlib
import re
import selectors
import socket
import subprocess
import sys
import time

case, directory = sys.argv[1:]
root = pathlib.Path(directory)
www = pathlib.Path(__file__).resolve().parents[1] / 'app/http-wire/www'


def stop(process):
    if process.poll() is None:
        process.terminate()
    process.communicate(timeout=3)


if case in ('python_ok', 'python_missing'):
    server = subprocess.Popen([sys.executable, '-u', '-m', 'http.server', '0',
                               '--bind', '127.0.0.1', '--protocol', 'HTTP/1.1', '--directory', str(www)],
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(server.stdout, selectors.EVENT_READ)
            assert ready.select(3)
        port = re.search(r' port (\d+) ', server.stdout.readline())[1]
        path = '/hello.txt' if case == 'python_ok' else '/missing'
        result = subprocess.run([str(root / 'http_length_client'), port, path], capture_output=True, timeout=5)
        assert result.returncode == 0, result.stderr
        line, body = result.stdout.split(b'\n', 1)
        code = 200 if case == 'python_ok' else 404
        assert line == f'status={code} bytes={len(body)}'.encode(), result.stdout
        if case == 'python_ok':
            assert body == (www / 'hello.txt').read_bytes()
    finally:
        stop(server)
elif case in ('invalid', 'refused'):
    if case == 'invalid':
        for args in ([], ['0'], ['65536'], ['8080', '/x\r\nHost: bad'], ['8080', '/#bad'], ['8080', '/', '0']):
            result = subprocess.run([str(root / 'http_length_client'), *args], capture_output=True, timeout=3)
            assert result.returncode == 1, args
    else:
        with socket.socket() as reserved:
            reserved.bind(('127.0.0.1', 0))
            result = subprocess.run([str(root / 'http_length_client'), str(reserved.getsockname()[1])], capture_output=True, timeout=3)
            assert result.returncode == 1 and b'connect' in result.stderr, result
else:
    good = b'HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello'
    fixtures = {
        'split': (good, None, b'hello'),
        'held_open': (good, None, b'hello'),
        'empty_body': (b'HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n', None, b''),
        'binary': (b'HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\n\x00\xffA\x00', None, b'\x00\xffA\x00'),
        'mixed_case': (b'HTTP/1.0 200 OK\r\ncOnTeNt-LeNgTh:\t5 \t\r\n\r\nhello', None, b'hello'),
        'truncated_headers': (b'HTTP/1.1 200 OK\r\nContent-Length: 5\r\n', b'truncated headers', None),
        'truncated_body': (b'HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhe', b'truncated body', None),
        'missing_length': (b'HTTP/1.1 200 OK\r\nX-Test: yes\r\n\r\nhello', b'missing Content-Length', None),
        'duplicate_length': (b'HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nhello', b'duplicate Content-Length', None),
        'conflicting_length': (b'HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 7\r\n\r\nhello', b'duplicate Content-Length', None),
        'negative_length': (b'HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\n', b'invalid or oversized Content-Length', None),
        'overflow_length': (b'HTTP/1.1 200 OK\r\nContent-Length: 999999999999999999999999\r\n\r\n', b'invalid or oversized Content-Length', None),
        'oversized_length': (b'HTTP/1.1 200 OK\r\nContent-Length: 4097\r\n\r\n', b'invalid or oversized Content-Length', None),
        'oversized_headers': (b'HTTP/1.1 200 OK\r\nX-Pad: ' + b'x' * 4096, b'headers exceed', None),
        'transfer_encoding': (b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n', b'encoded response', None),
        'te_and_cl': (b'HTTP/1.1 200 OK\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n', b'encoded response', None),
        'content_encoding': (b'HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 5\r\n\r\n', b'encoded response', None),
        'invalid_name': (b'HTTP/1.1 200 OK\r\nContent-Length : 5\r\n\r\nhello', b'invalid header name', None),
        'folded_header': (b'HTTP/1.1 200 OK\r\nContent-Length: 5\r\n more\r\n\r\nhello', b'invalid header line', None),
        'invalid_value': (b'HTTP/1.1 200 OK\r\nX-Test: a\x00b\r\nContent-Length: 5\r\n\r\nhello', b'invalid header value', None),
        'invalid_status': (b'BAD/1.1 200 OK\r\nContent-Length: 0\r\n\r\n', b'invalid status line', None),
        'unsupported_status': (b'HTTP/1.1 204 No Content\r\n\r\n', b'status outside', None),
        'surplus': (good + b'not a second response', None, b'hello'),
        'idle': (b'', b'receive deadline', None),
        'trickle': (b'', b'receive deadline', None),
        'body_limit': (b'HTTP/1.1 200 OK\r\nContent-Length: 4096\r\n\r\n' + b'x' * 4096, None, b'x' * 4096),
    }
    header_prefix = b'HTTP/1.1 200 OK\r\nContent-Length: 0\r\nX-Pad: '
    fixtures['header_limit'] = (header_prefix + b'x' * (4096 - len(header_prefix) - 4) + b'\r\n\r\n', None, b'')
    response, error, body = fixtures[case]
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(3)
        port = str(listener.getsockname()[1])
        client = subprocess.Popen([str(root / 'http_length_client'), port, '/hello.txt', '500'],
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
                        if case == 'split':
                            for byte in response:
                                peer.sendall(bytes([byte]))
                        else:
                            peer.sendall(response)
                    except (BrokenPipeError, ConnectionResetError):
                        assert error or case == 'surplus'
                if case.startswith('truncated_'):
                    peer.shutdown(socket.SHUT_WR)
                # All other fixtures stay open until the client has returned.
                out, err = client.communicate(timeout=3)
                if error:
                    assert client.returncode == 1 and error in err, (out, err)
                else:
                    assert client.returncode == 0 and out == f'status=200 bytes={len(body)}\n'.encode() + body, (out, err)
        finally:
            stop(client)
print('PASS HTTP length', case)
