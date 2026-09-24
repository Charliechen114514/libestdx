"""Exercise the button event chain in Renode: monitor-injected bounce must
produce 3 EXTI edges but exactly one debounced press/long/release cycle."""

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

EVENT_LINE = re.compile(rb"\[[^\]]*\]\[[^\]]*\]\[(edge|press|long|release)\] (\d+)$")


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


def drain(fd: int) -> bytes:
    received = bytearray()
    while True:
        ready, _, _ = select.select([fd], [], [], 0.2)
        if not ready:
            break
        chunk = os.read(fd, 4096)
        if not chunk:
            break
        received.extend(chunk)
    return bytes(received)


def parse_events(raw: bytes) -> list[tuple[str, int]]:
    events = []
    for line in raw.replace(b"\r\n", b"\n").split(b"\n"):
        match = EVENT_LINE.match(line.strip())
        if match:
            events.append((match.group(1).decode(), int(match.group(2))))
    return events


def expect(condition: bool, message: str, failures: list[str]) -> None:
    if not condition:
        failures.append(message)


def main() -> None:
    elf = pathlib.Path(sys.argv[1]).resolve()
    script = pathlib.Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="libestdx-renode-") as directory:
        pty_link = pathlib.Path(directory) / "button"
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

            def monitor(command_line: str) -> None:
                assert process.stdin is not None
                process.stdin.write(command_line + "\n")
                process.stdin.flush()

            read_until(fd, b"button events ready", 10)

            failures: list[str] = []

            # Scenario 1: bouncy press-and-hold (3 falling edges, held 0.9s).
            # release is the last event of the cycle, so waiting for it means
            # everything before it (edges, press, long) has already arrived.
            monitor("runMacro $bounce")
            raw = read_until(fd, b"[release]", 30)  # everything up to the last event
            events = parse_events(raw + drain(fd))
            names = [name for name, _ in events]
            stamps = {name: [t for n, t in events if n == name] for name in ("edge", "press", "long", "release")}
            expect(names.count("edge") == 3, f"bounce: expected 3 edges, got {names.count('edge')} in {events}", failures)
            expect(names.count("press") == 1, f"bounce: expected 1 press, got {names.count('press')} in {events}", failures)
            expect(names.count("long") == 1, f"bounce: expected 1 long, got {names.count('long')} in {events}", failures)
            expect(names.count("release") == 1, f"bounce: expected 1 release, got {names.count('release')} in {events}", failures)
            if stamps["press"] and stamps["long"] and stamps["release"]:
                press_t, long_t, release_t = stamps["press"][0], stamps["long"][0], stamps["release"][0]
                expect(press_t <= long_t < release_t, f"bounce: ordering press<=long<release violated: {events}", failures)
                expect(600 <= long_t - press_t <= 1000, f"bounce: long-press delay {long_t - press_t}ms outside 600..1000", failures)
                expect(0 <= release_t - long_t <= 500, f"bounce: release after long delayed {release_t - long_t}ms", failures)

            # Scenario 2: one clean short press (single falling edge).
            monitor("runMacro $press")
            raw = read_until(fd, b"[release]", 15)
            events = parse_events(raw + drain(fd))
            names = [name for name, _ in events]
            expect(names.count("edge") == 1, f"press: expected 1 edge, got {names.count('edge')} in {events}", failures)
            expect(names.count("press") == 1, f"press: expected 1 press, got {names.count('press')} in {events}", failures)
            expect(names.count("release") == 1, f"press: expected 1 release, got {names.count('release')} in {events}", failures)
            expect(names.count("long") == 0, f"press: unexpected long in {events}", failures)

            os.close(fd)
            if failures:
                for failure in failures:
                    print(f"FAIL: {failure}")
                print("--- renode tail ---")
                for line in list(log)[-40:]:
                    print(line.rstrip())
                sys.exit(1)
            print("button events check: PASS (3 edges vs 1 debounced press; long/release ordered)")
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()


if __name__ == "__main__":
    main()
