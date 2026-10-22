"""One-request HTTP server checks using independent HTTP and raw TCP clients."""
import http.client
import pathlib
import selectors
import socket
import subprocess
import sys

case, directory = sys.argv[1:]
root = pathlib.Path(directory)


def stop(process):
    if process.poll() is None:
        process.terminate()
    process.communicate(timeout=3)


if case == 'invalid':
    for args in ([], ['-1'], ['65536'], ['8081x'], ['0', '0'], ['0', '10001']):
        result = subprocess.run([str(root / 'http_small_server'), *args], capture_output=True, timeout=3)
        assert result.returncode == 1, args
elif case == 'conflict':
    with socket.socket() as reserved:
        reserved.bind(('127.0.0.1', 0))
        result = subprocess.run([str(root / 'http_small_server'), str(reserved.getsockname()[1])], capture_output=True, timeout=3)
        assert result.returncode == 1 and b'bind/listen' in result.stderr, result
else:
    server = subprocess.Popen([str(root / 'http_small_server'), '0', '500'],
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(server.stdout, selectors.EVENT_READ)
            assert ready.select(3), 'server not ready'
        line = server.stdout.readline().strip()
        assert line.startswith('listening 127.0.0.1:'), line
        port = int(line.rsplit(':', 1)[1])
        if case in ('get', 'missing'):
            path = '/hello.txt' if case == 'get' else '/missing'
            result = subprocess.run([str(root / 'http_length_client'), str(port), path], capture_output=True, timeout=3)
            body = b'Hello from Raspberry Pi.\n' if case == 'get' else b'Not found\n'
            code = 200 if case == 'get' else 404
            assert result.returncode == 0 and result.stdout == f'status={code} bytes={len(body)}\n'.encode() + body, result
        elif case == 'head':
            with socket.create_connection(('127.0.0.1', port), timeout=3) as peer:
                peer.sendall(f'HEAD /hello.txt HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\n\r\n'.encode())
                data = bytearray()
                while True:
                    chunk = peer.recv(128)
                    if not chunk:
                        break
                    data.extend(chunk)
                    assert len(data) <= 512
                headers, body = data.split(b'\r\n\r\n', 1)
                assert headers.startswith(b'HTTP/1.1 200 OK\r\n')
                assert b'Content-Length: 25' in headers and body == b''
        elif case in ('head_missing', 'method'):
            client = http.client.HTTPConnection('127.0.0.1', port, timeout=3)
            try:
                method = 'POST' if case == 'method' else 'HEAD'
                path = '/missing' if case == 'head_missing' else '/hello.txt'
                client.request(method, path)
                response = client.getresponse()
                assert response.status == (405 if case == 'method' else 404 if case == 'head_missing' else 200)
                expected = b'Method not allowed\n' if case == 'method' else b''
                assert response.read() == expected
                assert response.getheader('Content-Length') == ('19' if case == 'method' else '10' if case == 'head_missing' else '25')
                if case == 'method':
                    assert response.getheader('Allow') == 'GET, HEAD'
            finally:
                client.close()
        else:
            host = f'Host: 127.0.0.1:{port}\r\n'.encode()
            first = b'GET /hello.txt HTTP/1.1\r\n'
            good = first + host + b'\r\n'
            fixtures = {
                'split': (good, None),
                'mixed_case': (first + f'hOsT:\t127.0.0.1:{port} \t\r\ncontent-length: 0\r\n\r\n'.encode(), None),
                'missing_host': (first + b'\r\n', 'missing Host'),
                'duplicate_host': (first + host + host + b'\r\n', 'duplicate or unsupported Host'),
                'wrong_host': (first + b'Host: example.invalid\r\n\r\n', 'duplicate or unsupported Host'),
                'transfer_encoding': (first + host + b'Transfer-Encoding: chunked\r\n\r\n', 'request feature unsupported'),
                'request_body': (first + host + b'Content-Length: 1\r\n\r\n', 'request body'),
                'duplicate_length': (first + host + b'Content-Length: 0\r\nContent-Length: 0\r\n\r\n', 'duplicate length'),
                'malformed_header': (first + host + b'Bad Name: yes\r\n\r\n', 'invalid header name'),
                'bad_request_line': (b'GET /hello.txt HTTP/1.0\r\n' + host + b'\r\n', 'invalid request line'),
                'oversized': (first + b'X-Pad: ' + b'x' * 4096, 'request exceeds'),
                'truncated': (first, 'truncated request'),
                'idle': (b'', 'request deadline'),
            }
            request, error = fixtures[case]
            with socket.create_connection(('127.0.0.1', port), timeout=3) as peer:
                if case == 'split':
                    for byte in request:
                        peer.sendall(bytes([byte]))
                else:
                    peer.sendall(request)
                if case == 'truncated':
                    peer.shutdown(socket.SHUT_WR)
                if not error:
                    data = bytearray()
                    while True:
                        chunk = peer.recv(128)
                        if not chunk:
                            break
                        data.extend(chunk)
                        assert len(data) <= 512
                    headers, body = data.split(b'\r\n\r\n', 1)
                    assert headers.startswith(b'HTTP/1.1 200 OK\r\n') and b'Content-Length: 25' in headers
                    assert body == b'Hello from Raspberry Pi.\n'
                else:
                    out, err = server.communicate(timeout=3)
                    assert server.returncode == 1 and error in err, (out, err)
        if case in ('get', 'missing', 'head', 'head_missing', 'method', 'split', 'mixed_case'):
            out, err = server.communicate(timeout=3)
            assert server.returncode == 0 and 'response=' in out, (out, err)
    finally:
        stop(server)
print('PASS HTTP server', case)
