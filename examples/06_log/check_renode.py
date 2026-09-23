"""Verify the one-way log stream of examples/07_log in Renode through a PTY.

Byte-exact notes: level names are width-5 and left-padded, so the lines
carry "Warn " and "Info " WITH a trailing space. The timestamp segment is
millisecond-granular and matched by regex, not by literals.
"""

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


def main() -> None:
    elf = pathlib.Path(sys.argv[1]).resolve()
    script = pathlib.Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="libestdx-renode-") as directory:
        pty_link = pathlib.Path(directory) / "uart"
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
            deadline = time.monotonic() + 20
            while not pty_link.exists():
                if process.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError("Renode did not create the UART PTY")
                time.sleep(0.02)

            fd = os.open(pty_link, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            try:
                tty.setraw(fd)
                received = bytearray()
                for marker, timeout in [
                    (b"[Error][boot] libestdx 07_log example\r\n", 10),
                    (b"[Warn ][boot] MinLevel=Info, clock=HalTickClock\r\n", 5),
                    (b"[Info ][boot] watch PTY /tmp/libestdx-log\r\n", 5),
                    (b"[Info ][led] blink=1\r\n", 15),
                    (b"[Info ][ledf] n=1 hex=0x1\r\n", 5),
                ]:
                    received += read_until(fd, marker, timeout)

                # Compile-time pruning: the debug line must not exist anywhere.
                if b"[Debug" in bytes(received):
                    raise AssertionError("pruned debug line appeared in the stream")
                if b"must-not-survive" in bytes(received):
                    raise AssertionError("debug marker literal appeared in the stream")
                # Timestamps: every line starts with [Nms] segments.
                if not re.search(rb"\[\d+ms\]\[(Error|Warn |Info )\]\[", bytes(received)):
                    raise AssertionError("no timestamped log line found")

                print(f"Renode log stream: banner, blink, pruning and timestamps passed")
            finally:
                os.close(fd)
        except BaseException:
            print("Renode output:\n" + "".join(log), file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.stdin.write("quit\n")
                process.stdin.flush()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


if __name__ == "__main__":
    main()
