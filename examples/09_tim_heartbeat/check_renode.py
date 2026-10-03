"""TIM heartbeat check: 1Hz beats with ~1000ms spacing while the main loop
sleeps in WFI (time as the third event source)."""

import collections
import os
import pathlib
import re
import select
import subprocess
import sys
import tempfile
import threading
import time
import tty

BEAT_LINE = re.compile(rb"\[(\d+)ms\]\[[^\]]*\]\[beat\] (\d+)$", re.MULTILINE)


def read_until(fd: int, expected: bytes, timeout: float) -> bytes:
    received = bytearray()
    deadline = time.monotonic() + timeout
    while expected not in received:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(f"expected {expected!r}, received {bytes(received)!r}")
        ready, _, _ = select.select([fd], [], [], remaining)
        if ready:
            received.extend(os.read(fd, 4096))
    return bytes(received)


def drain(fd: int, settle: float = 0.5) -> bytes:
    received = bytearray()
    while True:
        ready, _, _ = select.select([fd], [], [], settle)
        if not ready:
            break
        chunk = os.read(fd, 4096)
        if not chunk:
            break
        received.extend(chunk)
    return bytes(received)


def expect(condition: bool, message: str, failures: list) -> None:
    if not condition:
        failures.append(message)


def main() -> None:
    elf = pathlib.Path(sys.argv[1]).resolve()
    script = pathlib.Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="libestdx-renode-") as directory:
        pty_link = pathlib.Path(directory) / "heartbeat"
        environment = dict(os.environ, XDG_CONFIG_HOME=str(pathlib.Path(directory) / "config"))
        command = [
            "renode", "--console", "--disable-xwt",
            "-e", f"$bin=@{elf}",
            "-e", f'$uart_pty="{pty_link}"',
            "-e", f"include @{script}",
        ]
        process = subprocess.Popen(
            command, cwd=script.parents[2], env=environment,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, bufsize=1,
        )
        log = collections.deque(maxlen=80)
        reader = threading.Thread(target=lambda: log.extend(iter(process.stdout.readline, "")), daemon=True)
        reader.start()

        def monitor(command_line: str) -> None:
            assert process.stdin is not None
            process.stdin.write(command_line + "\n")
            process.stdin.flush()

        try:
            deadline = time.monotonic() + 10
            while not pty_link.exists() and time.monotonic() < deadline:
                time.sleep(0.05)
            if not pty_link.exists():
                raise TimeoutError(f"PTY never appeared: {pty_link}")

            fd = os.open(str(pty_link), os.O_RDWR | os.O_NOCTTY)
            tty.setraw(fd)

            read_until(fd, b"heartbeat ready", 10)

            failures: list = []

            monitor("pause")
            monitor('emulation RunFor "5.2"')
            monitor("start")
            raw = read_until(fd, b"[beat] 5", 30) + drain(fd)

            beats = [(int(m.group(1)), int(m.group(2)))
                     for m in BEAT_LINE.finditer(raw.replace(b"\r\n", b"\n"))]
            numbers = [n for _, n in beats]
            expect(numbers == [1, 2, 3, 4, 5], f"expected beats 1..5, got {numbers}", failures)
            stamps = [t for t, _ in beats]
            for first, second in zip(stamps, stamps[1:]):
                gap = second - first
                expect(900 <= gap <= 1100, f"beat gap {gap}ms outside 900..1100", failures)

            os.close(fd)
            if failures:
                for failure in failures:
                    print(f"FAIL: {failure}")
                print("--- renode tail ---")
                for line in list(log)[-40:]:
                    print(line.rstrip())
                sys.exit(1)
            print("tim heartbeat check: PASS (1Hz beats under WFI main loop, spacing 1000ms±10%)")
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()


if __name__ == "__main__":
    main()
