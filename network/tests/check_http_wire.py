"""Raw HTTP request checks against Python's server and independent TCP peers."""
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
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(server.stdout, selectors.EVENT_READ)
            assert ready.select(3), 'HTTP server not ready'
        line = server.stdout.readline()
        port = re.search(r' port (\d+) ', line)[1]
        path = '/hello.txt' if case == 'python_ok' else '/missing'
        result = subprocess.run([str(root / 'http_wire_client'), port, path], capture_output=True, timeout=6)
        assert result.returncode == 0, result.stderr
        headers, body = result.stdout.split(b'\r\n\r\n', 1)
        assert headers.startswith(b'HTTP/1.1 200 ' if case == 'python_ok' else b'HTTP/1.1 404 '), headers
        if case == 'python_ok':
            assert body == (www / 'hello.txt').read_bytes()
    finally:
        stop(server)
elif case == 'invalid':
    for args in ([], ['0'], ['-1'], ['65536'], ['8080x'], ['8080', 'bad'],
                 ['8080', '/bad\r\nX: y'], ['8080', '/has space'], ['8080', '/#fragment'],
                 ['8080', '/', '0'], ['8080', '/', '10001']):
        result = subprocess.run([str(root / 'http_wire_client'), *args], capture_output=True, timeout=3)
        assert result.returncode == 1, args
elif case == 'refused':
    with socket.socket() as reserved:
        reserved.bind(('127.0.0.1', 0))
        result = subprocess.run([str(root / 'http_wire_client'), str(reserved.getsockname()[1])], capture_output=True, timeout=4)
        assert result.returncode == 1 and b'connect' in result.stderr, result
else:
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(3)
        port = str(listener.getsockname()[1])
        client = subprocess.Popen([str(root / 'http_wire_client'), port, '/hello.txt', '500'],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            peer, _ = listener.accept()
            with peer:
                peer.settimeout(3)
                request = bytearray()
                while not request.endswith(b'\r\n\r\n'):
                    chunk = peer.recv(128)
                    assert chunk, 'client used EOF instead of an HTTP request'
                    request.extend(chunk)
                    assert len(request) <= 512
                assert request == f'GET /hello.txt HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nConnection: close\r\n\r\n'.encode()
                peer.setblocking(False)
                try:
                    assert peer.recv(1, socket.MSG_PEEK) != b'', 'GET must not rely on write-side EOF'
                except BlockingIOError:
                    pass
                peer.settimeout(3)
                # The fixture replies before the client's write side closes.
                response = b'HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello'
                if case == 'split':
                    for byte in response:
                        peer.sendall(bytes([byte]))
                elif case == 'binary':
                    response = b'HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\n\x00\xffA\x00'
                    peer.sendall(response)
                elif case == 'oversized':
                    try:
                        peer.sendall(b'x' * 8193)
                    except (BrokenPipeError, ConnectionResetError):
                        pass
                elif case == 'trickle':
                    end = time.monotonic() + 1.8
                    while client.poll() is None and time.monotonic() < end:
                        try:
                            peer.sendall(b'x')
                        except (BrokenPipeError, ConnectionResetError):
                            break
                        time.sleep(0.05)
                    assert client.poll() is not None, 'incoming data renewed receive budget'
                elif case == 'hold_open':
                    peer.sendall(response)
                elif case not in ('idle', 'empty'):
                    raise ValueError(case)
                if case not in ('idle', 'hold_open', 'trickle'):
                    peer.shutdown(socket.SHUT_WR)
                out, err = client.communicate(timeout=3)
                if case in ('split', 'binary'):
                    assert client.returncode == 0 and out == response, (out, err)
                else:
                    message = b'response exceeds' if case == 'oversized' else b'empty response' if case == 'empty' else b'receive deadline'
                    assert client.returncode == 1 and message in err, (out, err)
        finally:
            stop(client)
print('PASS HTTP wire', case)
