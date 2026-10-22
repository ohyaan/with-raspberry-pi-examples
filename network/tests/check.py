"""Finite loopback integration checks; no fixed ports or external network."""
import pathlib
import selectors
import socket
import subprocess
import sys

case, directory = sys.argv[1:]
root = pathlib.Path(directory)


def run(name, *args):
    return subprocess.run([str(root / name), *args], text=True, capture_output=True, timeout=6)


def expect(name, output, *args):
    result = run(name, *args)
    assert result.returncode == 0, (name, result.stderr)
    assert result.stdout == output, (name, result.stdout, output)


def exchange(payload, use_client=False, reject=False):
    server = subprocess.Popen([str(root / 'udp_echo_server'), '0'],
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        with selectors.DefaultSelector() as ready:
            ready.register(server.stdout, selectors.EVENT_READ)
            assert ready.select(3), 'server did not become ready'
        line = server.stdout.readline().strip()
        assert line.startswith('listening 127.0.0.1:'), line
        port = int(line.rsplit(':', 1)[1])
        if use_client:
            expect('udp_echo_client', f'reply bytes={len(payload)} data={payload.decode()}\n',
                   str(port), payload.decode())
        else:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as peer:
                peer.settimeout(3)
                peer.sendto(payload, ('127.0.0.1', port))
                if not reject:
                    reply, source = peer.recvfrom(2048)
                    assert reply == payload
                    assert source == ('127.0.0.1', port)
        out, err = server.communicate(timeout=3)
        if reject:
            assert server.returncode != 0 and 'datagram too large' in err, (out, err)
        else:
            assert server.returncode == 0, err
            assert out == f'echoed bytes={len(payload)}\n', out
    finally:
        if server.poll() is None:
            server.terminate()
        server.communicate(timeout=3)


if case == 'hello':
    expect('hello_network', 'Network lab ready\nC++20: yes\n')
elif case in ('ipv4', 'ipv6'):
    address, family = ('127.0.0.1', 'IPv4') if case == 'ipv4' else ('::1', 'IPv6')
    expect('addresses', f'{family} address={address} port=9000\nUDP socket created; no packets sent\n', address)
elif case in ('echo', 'empty', 'limit'):
    exchange({'echo': b'hello', 'empty': b'', 'limit': b'x' * 1024}[case], use_client=True)
elif case == 'binary':
    exchange(b'\x00\xffA\x00B')
elif case == 'oversized':
    exchange(b'x' * 1025, reject=True)
    result = run('udp_echo_client', '9000', 'x' * 1025)
    assert result.returncode != 0 and 'exceeds 1024' in result.stderr
elif case == 'truncation':
    expect('udp_boundaries', 'datagram=1 bytes=1 copied=1 data=A truncated=no\n'
           'datagram=2 bytes=6 copied=3 data=BCD truncated=yes\n'
           'datagram=3 bytes=0 copied=0 data= truncated=no\n')
elif case in ('late', 'loss'):
    expect('udp_sequence', 'id=1 new\nid=3 gap\nid=3 duplicate\n' +
           ('id=2 late\nmissing=none\n' if case == 'late' else 'missing=2\n'), case)
elif case == 'bad_port':
    for invalid in ('-1', '65536', '9000x', '', '999999999999999999999999999999'):
        for name in ('udp_echo_client', 'udp_echo_server'):
            result = run(name, invalid)
            assert result.returncode != 0 and 'port must be' in result.stderr, result
    assert run('udp_echo_client', '0').returncode != 0
    assert run('udp_sequence', 'unknown').returncode != 0
elif case == 'occupied':
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reserved:
        reserved.bind(('127.0.0.1', 0))
        result = run('udp_echo_server', str(reserved.getsockname()[1]))
        assert result.returncode != 0 and 'bind' in result.stderr
elif case == 'timeout':
    # A bound peer accepts the datagram but deliberately never replies.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as silent:
        silent.bind(('127.0.0.1', 0))
        result = run('udp_echo_client', str(silent.getsockname()[1]))
        assert result.returncode != 0 and 'receive timeout' in result.stderr
else:
    raise ValueError(case)
print('PASS', case)
