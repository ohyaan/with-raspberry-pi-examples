"""Sequential HTTP requests on one connection, with no implicit retry."""
import pathlib
import re
import selectors
import socket
import subprocess
import sys

case, directory = sys.argv[1:]
root = pathlib.Path(directory)
www = pathlib.Path(__file__).resolve().parents[1] / 'app/http-wire/www'


def stop(process):
    if process.poll() is None:
        process.terminate()
    process.communicate(timeout=3)


def read_request(peer):
    data = bytearray()
    while not data.endswith(b'\r\n\r\n'):
        chunk = peer.recv(1)
        assert chunk, 'request ended early'
        data.extend(chunk)
        assert len(data) <= 512
    return bytes(data)


if case == 'fixture':
    server = subprocess.Popen([sys.executable, '-u', '-m', 'http.server', '0', '--bind', '127.0.0.1',
                               '--protocol', 'HTTP/1.1', '--directory', str(www)],
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(server.stdout, selectors.EVENT_READ)
            assert ready.select(3)
        port = re.search(r' port (\d+) ', server.stdout.readline())[1]
        result = subprocess.run([str(root / 'http_reuse_client'), port], capture_output=True, timeout=5)
        first = b'response=1 status=200 bytes=25\n' + (www / 'hello.txt').read_bytes()
        assert result.returncode == 0 and result.stdout.startswith(first), result
        line, body = result.stdout[len(first):].split(b'\n', 1)
        assert line == f'response=2 status=404 bytes={len(body)}'.encode(), result.stdout
    finally:
        stop(server)
elif case in ('invalid', 'refused'):
    if case == 'invalid':
        for args in ([], ['0'], ['65536'], ['8080', '0'], ['8080', '10001']):
            result = subprocess.run([str(root / 'http_reuse_client'), *args], capture_output=True, timeout=3)
            assert result.returncode == 1, args
    else:
        with socket.socket() as reserved:
            reserved.bind(('127.0.0.1', 0))
            result = subprocess.run([str(root / 'http_reuse_client'), str(reserved.getsockname()[1])], capture_output=True, timeout=3)
            assert result.returncode == 1 and b'connect' in result.stderr, result
else:
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(2)
        listener.settimeout(3)
        port = str(listener.getsockname()[1])
        client = subprocess.Popen([str(root / 'http_reuse_client'), port, '500'],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        error = None
        first_body = b'' if case == 'empty_first' else b'one'
        first = f'HTTP/1.1 200 OK\r\nContent-Length: {len(first_body)}\r\n\r\n'.encode() + first_body
        second = b'HTTP/1.1 404 Not Found\r\nContent-Length: 3\r\nConnection: close\r\n\r\ntwo'
        try:
            peer, _ = listener.accept()
            with peer:
                peer.settimeout(3)
                request = read_request(peer)
                assert request == f'GET /hello.txt HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nConnection: keep-alive\r\n\r\n'.encode()
                peer.setblocking(False)
                try:
                    peer.recv(1, socket.MSG_PEEK)
                    raise AssertionError('client pipelined or closed before first response')
                except BlockingIOError:
                    pass
                peer.settimeout(3)
                if case == 'first_close':
                    first = b'HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\none'
                    error = b'peer requested close'
                elif case == 'unsolicited':
                    first += b'unsolicited data'
                    error = b'unsolicited'
                elif case == 'missing_length':
                    first = b'HTTP/1.1 200 OK\r\nX-Test: yes\r\n\r\none'
                    error = b'missing Content-Length'
                elif case == 'duplicate_length':
                    first = b'HTTP/1.1 200 OK\r\nContent-Length: 3\r\nContent-Length: 3\r\n\r\none'
                    error = b'duplicate Content-Length'
                elif case == 'idle_first':
                    first = b''
                    error = b'receive deadline'
                if case == 'one_byte':
                    for byte in first:
                        peer.sendall(bytes([byte]))
                else:
                    peer.sendall(first)
                if not error:
                    request = read_request(peer)
                    assert request == f'GET /missing HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nConnection: close\r\n\r\n'.encode()
                    if case == 'premature_eof':
                        second = b'HTTP/1.1 404 Not Found\r\nContent-Length: 3\r\n\r\nt'
                        error = b'truncated body'
                    elif case == 'wrong_second_status':
                        second = b'HTTP/1.1 500 Error\r\nContent-Length: 0\r\n\r\n'
                        error = b'status outside'
                    elif case == 'idle_second':
                        second = b''
                        error = b'receive deadline'
                    if case == 'one_byte':
                        for byte in second:
                            peer.sendall(bytes([byte]))
                    else:
                        peer.sendall(second)
                    if case == 'premature_eof':
                        peer.shutdown(socket.SHUT_WR)
                out, err = client.communicate(timeout=3)
                if error:
                    assert client.returncode == 1 and error in err, (out, err)
                else:
                    expected = f'response=1 status=200 bytes={len(first_body)}\n'.encode() + first_body + b'response=2 status=404 bytes=3\ntwo'
                    assert client.returncode == 0 and out == expected, (out, err)
            listener.settimeout(0.1)
            try:
                extra, _ = listener.accept()
                extra.close()
                raise AssertionError('client opened another connection or retried')
            except socket.timeout:
                pass
        finally:
            stop(client)
print('PASS HTTP reuse', case)
