"""Confirm-debounce check: bounce collapses to one confirmed touch, a 5ms
glitch produces no event at all, and the same ButtonFsm (fed by confirmed
edges + timer ticks) still emits press/long/release in order."""

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

EVENT_LINE = re.compile(rb"\[(\d+)ms\]\[[^\]]*\]\[(touch|leave|press|long|release)\] ?(\d+)?$",
                        re.MULTILINE)


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


def parse_events(raw: bytes) -> list:
    return [(match.group(2).decode(), int(match.group(1)), int(match.group(3) or 0))
            for match in EVENT_LINE.finditer(raw.replace(b"\r\n", b"\n"))]


def expect(condition: bool, message: str, failures: list) -> None:
    if not condition:
        failures.append(message)


def main() -> None:
    elf = pathlib.Path(sys.argv[1]).resolve()
    script = pathlib.Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="libestdx-renode-") as directory:
        pty_link = pathlib.Path(directory) / "confirm"
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

            read_until(fd, b"button confirm ready", 10)

            failures: list = []

            def counts(events):
                return {name: [e for n, e, _ in events if n == name] for name in
                        ("touch", "leave", "press", "long", "release")}

            def stamps(events, name):
                return [ms for n, _, ms in events if n == name]

            # Scenario 1: bounce collapses to one confirmed touch + full fsm cycle.
            monitor("runMacro $bounce")
            raw = read_until(fd, b"[release]", 30) + drain(fd)
            events = parse_events(raw)
            c = counts(events)
            expect(len(c["touch"]) == 1, f"bounce: expected 1 touch, got {len(c['touch'])} in {events}", failures)
            expect(len(c["press"]) == 1, f"bounce: expected 1 press, got {len(c['press'])} in {events}", failures)
            expect(len(c["long"]) == 1, f"bounce: expected 1 long, got {len(c['long'])} in {events}", failures)
            expect(len(c["release"]) == 1, f"bounce: expected 1 release, got {len(c['release'])} in {events}", failures)
            press_t, long_t, release_t = stamps(events, "press"), stamps(events, "long"), stamps(events, "release")
            if press_t and long_t and release_t:
                expect(press_t[0] <= long_t[0] < release_t[0],
                       f"bounce: ordering violated: {events}", failures)
                expect(600 <= long_t[0] - press_t[0] <= 1000,
                       f"bounce: long delay {long_t[0] - press_t[0]}ms outside 600..1000", failures)
            # touch 确认延迟: 最后一个沿后 ~20ms, 即首沿后 60ms 内
            touch_t = stamps(events, "touch")
            if touch_t and press_t:
                expect(0 <= press_t[0] - touch_t[0] <= 5,
                       f"bounce: press lagging confirm: {events}", failures)

            # Scenario 2: 5ms glitch must produce nothing.
            monitor("runMacro $glitch")
            raw = drain(fd, 2.0)
            events = parse_events(raw)
            expect(not events, f"glitch: expected silence, got {events}", failures)

            # Scenario 3: clean press, no long.
            monitor("runMacro $press")
            raw = read_until(fd, b"[release]", 15) + drain(fd)
            events = parse_events(raw)
            c = counts(events)
            expect(len(c["touch"]) == 1 and len(c["press"]) == 1 and len(c["release"]) == 1,
                   f"press: expected touch+press+release, got {events}", failures)
            expect(len(c["long"]) == 0, f"press: unexpected long in {events}", failures)

            os.close(fd)
            if failures:
                for failure in failures:
                    print(f"FAIL: {failure}")
                print("--- renode tail ---")
                for line in list(log)[-40:]:
                    print(line.rstrip())
                sys.exit(1)
            print("button confirm check: PASS (bounce->1 confirm, glitch->silence, fsm fed by source events)")
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()


if __name__ == "__main__":
    main()
