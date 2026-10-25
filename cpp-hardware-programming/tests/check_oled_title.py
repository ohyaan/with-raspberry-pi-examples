"""Bounded software tests: preview rendering plus a link-time fake I2C bus."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

CASE, PROGRAM, FIXTURE = sys.argv[1:]
DEVICE = '/unverified-device'


def run(*args, program=PROGRAM, env=None):
    return subprocess.run([program, *args], capture_output=True, timeout=5, env=env)


def image(frame):
    result = run(DEVICE, '0x3c', '--preview', str(frame))
    assert result.returncode == 0 and not result.stderr, result
    assert result.stdout.startswith(b'P4\n128 64\n')
    raw = result.stdout[len(b'P4\n128 64\n'):]
    assert len(raw) == 1024
    return raw


def simulate(**options):
    with tempfile.TemporaryDirectory(prefix='oled-title-test-') as directory:
        path = Path(directory) / 'bus.log'
        env = dict(os.environ, OLED_TEST_LOG=str(path), **options)
        result = run(DEVICE, '0x3c', '--show', '--frames', '3', '--fps', '30',
                     program=FIXTURE, env=env)
        return result, path.read_text().splitlines()


def dry():
    for address in ('0x3c', '0x3d'):
        result = run(DEVICE, address)
        assert result.returncode == 0 and not result.stderr, result
        assert b'128 frames, target 16 fps, 1024 framebuffer bytes' in result.stdout
        assert b'No I2C device opened.' in result.stdout
        assert b'checksum=2626571566\n' in result.stdout
        assert run(DEVICE, address).stdout == result.stdout


def options():
    cases = [(), (DEVICE,), (DEVICE, '0x78'), (DEVICE, '3c')]
    for suffix in (['--wrong'], ['--show', '--off'], ['--show', '--show'],
                   ['--preview'], ['--preview', '128'], ['--preview', '-1'],
                   ['--preview', 'x'], ['--fps'], ['--fps', '0'], ['--fps', '31'],
                   ['--frames', '0'], ['--frames', '601'], ['--frames', '10x'],
                   ['--fps', '1'], ['--frames', '1', '--frames', '2'],
                   ['--fps', '2', '--fps', '3'], ['--off', '--frames', '1'],
                   ['--preview', '64', '--fps', '16']):
        cases.append((DEVICE, '0x3c', *suffix))
    for args in cases:
        result = run(*args)
        assert result.returncode == 1 and result.stderr and not result.stdout, (args, result)
    result = run(DEVICE, '0x3c', '--frames', '30', '--fps', '1')
    assert result.returncode == 0 and b'30 frames, target 1 fps' in result.stdout


def previews():
    frames = [image(frame) for frame in (0, 16, 32, 48, 64, 95, 96, 111, 112, 127)]
    assert len(set(frames)) == len(frames)
    counts = [sum(byte.bit_count() for byte in raw) for raw in frames]
    assert counts[0] < 30 and counts[4] > 500 and counts[-1] < 30, counts
    # The held title region is unchanged while stars outside it still move.
    assert frames[3][10 * 16:50 * 16] == frames[4][10 * 16:50 * 16]
    assert frames[4][10 * 16:50 * 16] == frames[5][10 * 16:50 * 16]
    assert hashlib.sha256(frames[4]).hexdigest() == (
        '2e501b60163599a714edccaceb8eb7a443818be52a4f605fd1fafa3962290368')


def missing_device():
    for mode in ('--show', '--off'):
        result = run(DEVICE, '0x3c', mode)
        assert result.returncode == 1 and b'Open I2C:' in result.stderr, result


def bus_frames():
    result, events = simulate()
    assert result.returncode == 0 and not result.stderr, result
    assert b'frames_sent=3 target_fps=30 stopped=complete' in result.stdout
    assert events[:2] == ['OPEN', 'ADDRESS 60'] and events[-1] == 'CLOSE', events
    writes = [bytes.fromhex(line[6:]) for line in events if line.startswith('WRITE ')]
    assert writes[0] == b'\x00\xae' and writes[-1] == b'\x00\xae'
    assert writes.count(b'\x00\xaf') == 1
    assert writes.count(b'\x00\x21') == 3
    data = b''.join(item[1:] for item in writes if item[0] == 0x40)
    assert len(data) == 3 * 1024
    # Independently convert row-major PBM pixels into SSD1306 page bytes.
    for frame in range(3):
        raw = image(frame)
        page = bytearray(1024)
        for y in range(64):
            for x in range(128):
                if raw[y * 16 + x // 8] & (0x80 >> (x % 8)):
                    page[x + y // 8 * 128] |= 1 << (y % 8)
        assert data[frame * 1024:(frame + 1) * 1024] == page


def bus_failure():
    result, events = simulate(OLED_TEST_FAIL='1')
    assert result.returncode == 1 and b'I2C write:' in result.stderr, result
    assert events[-2:] == ['WRITE 00ae', 'CLOSE'], events[-3:]
    assert b'frames_sent=' not in result.stdout


def bus_short_write():
    result, events = simulate(OLED_TEST_SHORT='1')
    assert result.returncode == 1 and b'Short I2C write' in result.stderr, result
    assert events[-2:] == ['WRITE 00ae', 'CLOSE']


def bus_signal():
    result, events = simulate(OLED_TEST_SIGNAL='1')
    assert result.returncode == 130 and b'stopped=signal' in result.stdout, result
    assert events[-2:] == ['WRITE 00ae', 'CLOSE']


def bus_address_failure():
    result, events = simulate(OLED_TEST_ADDRESS_FAIL='1')
    assert result.returncode == 1 and b'Select I2C address:' in result.stderr, result
    assert events == ['OPEN', 'ADDRESS 60', 'CLOSE'], events


CASES = {name: value for name, value in list(globals().items())
         if name in ('dry', 'options', 'previews', 'missing_device', 'bus_frames',
                     'bus_failure', 'bus_short_write', 'bus_signal', 'bus_address_failure')}
CASES[CASE]()
print('PASS oled_title.' + CASE)
