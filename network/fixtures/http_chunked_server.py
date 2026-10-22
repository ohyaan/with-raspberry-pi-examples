"""One-request loopback fixture for the chunked-body lesson, not a service."""
import http.server
import sys


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def setup(self):
        super().setup()
        self.connection.settimeout(3)

    def do_GET(self):
        found = self.path == '/hello.txt'
        self.send_response(200 if found else 404)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Transfer-Encoding', 'chunked')
        self.send_header('Connection', 'close')
        self.end_headers()
        chunks = (b'Hello ', b'from Pi.\n') if found else (b'Not found\n',)
        for chunk in chunks:
            self.wfile.write(f'{len(chunk):x}\r\n'.encode() + chunk + b'\r\n')
            self.wfile.flush()
        self.wfile.write(b'0\r\n\r\n')
        self.wfile.flush()
        self.close_connection = True


if __name__ == '__main__':
    if len(sys.argv) != 2 or not sys.argv[1].isascii() or not sys.argv[1].isdecimal() or not 0 <= int(sys.argv[1]) <= 65535:
        raise SystemExit('usage: http_chunked_server.py port (0..65535)')
    with http.server.HTTPServer(('127.0.0.1', int(sys.argv[1])), Handler) as server:
        server.timeout = 30
        print(f'listening 127.0.0.1:{server.server_port}', flush=True)
        server.handle_request()
