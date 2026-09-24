"""Exercise the interrupt-driven ring console in Renode.

Headline scenario: while the main loop is blocked in `sleep 300` (HAL_Delay),
a burst of `ping` + a long `echo` arrives. The ISR keeps feeding the ring;
once the main loop wakes, responses come back complete and in order —
something the polling console (05_uart) cannot guarantee.
"""

import collections
import os
import pathlib
import select
import subprocess
import sys
import tempfile
import threading
import time
import tty


def read_until(fd: int, expected: bytes, timeout: float) -> bytes:
    received = bytearray()
    deadline = time.monotonic() + timeout
    while expected not in received:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(f"expected {expected!r}, got tail {bytes(received[-100:])!r}")
        ready, _, _ = select.select([fd], [], [], remaining)
        if ready:
            received.extend(os.read(fd, 4096))
    return bytes(received)


def expect(condition: bool, message: str, failures: list[str]) -> None:
    if not condition:
        failures.append(message)


def main() -> None:
    elf = pathlib.Path(sys.argv[1]).resolve()
    script = pathlib.Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="libestdx-renode-") as directory:
        pty_link = pathlib.Path(directory) / "ring"
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

        try:
            deadline = time.monotonic() + 10
            while not pty_link.exists() and time.monotonic() < deadline:
                time.sleep(0.05)
            if not pty_link.exists():
                raise TimeoutError(f"PTY never appeared: {pty_link}")

            fd = os.open(str(pty_link), os.O_RDWR | os.O_NOCTTY)
            tty.setraw(fd)

            failures: list[str] = []

            read_until(fd, b"ring console ready", 10)

            # Sanity: plain ping round-trip.
            os.write(fd, b"ping\r")
            out = read_until(fd, b"pong\r\n", 5)
            expect(b"pong" in out, f"ping: no pong in {out!r}", failures)
            read_until(fd, b"> ", 5)

            # Headline: burst while the main loop sleeps 300ms.
            # Keep "echo " + canary within the 48-char line buffer.
            canary = b"burst-canary-012345678901234567890123456789"
            os.write(fd, b"sleep 300\r" + b"ping\r" + b"echo " + canary + b"\r")
            # Response tail: ... busy ... done ... pong ... canary ...
            out = read_until(fd, canary + b"\r\n> ", 15)
            out += bytearray(os.read(fd, 4096)) if select.select([fd], [], [], 0.3)[0] else b""
            out = bytes(out)
            busy_at, done_at = out.find(b"busy"), out.find(b"done")
            pong_at = out.find(b"pong\r\n", max(done_at, 0))
            canary_at = out.rfind(canary)
            expect(busy_at != -1 and done_at != -1, f"burst: sleep markers missing in {out[-160:]!r}", failures)
            expect(pong_at > done_at, f"burst: pong did not survive the sleep (order wrong): {out[-160:]!r}", failures)
            expect(canary_at > pong_at, f"burst: echo canary lost or out of order: {out[-160:]!r}", failures)
            expect(out.count(canary) >= 1, f"burst: canary text incomplete: {out[-160:]!r}", failures)

            # Error paths carry the reason, not silence.
            os.write(fd, b"frobnicate\r")
            out = read_until(fd, b"> ", 5)
            expect(b"unknown command: frobnicate" in out, f"unknown: wrong reply {out!r}", failures)
            os.write(fd, b"sleep xyz\r")
            out = read_until(fd, b"> ", 5)
            expect(b"not a number: xyz" in out, f"badnum: wrong reply {out!r}", failures)

            os.close(fd)
            if failures:
                for failure in failures:
                    print(f"FAIL: {failure}")
                print("--- renode tail ---")
                for line in list(log)[-40:]:
                    print(line.rstrip())
                sys.exit(1)
            print("ring console check: PASS (burst survives 300ms busy main loop, in order; errors carry reasons)")
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()


if __name__ == "__main__":
    main()
