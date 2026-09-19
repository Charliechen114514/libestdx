"""Exercise the UART command console in Renode through a PTY terminal."""

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
            raise TimeoutError(f"expected {expected!r}, received {bytes(received)!r}")
        ready, _, _ = select.select([fd], [], [], remaining)
        if ready:
            received.extend(os.read(fd, 4096))
    return bytes(received)


def run_command(fd: int, command: bytes, expected: bytes) -> None:
    for value in command:
        char = bytes([value])
        os.write(fd, char)
        echoed = read_until(fd, char, 5)
        if echoed != char:
            raise AssertionError(f"input echo: expected {char!r}, received {echoed!r}")
    os.write(fd, b"\r")
    response = read_until(fd, b"> ", 5)
    wanted = b"\r\n" + expected + b"> "
    if response != wanted:
        raise AssertionError(f"{command!r}: expected {wanted!r}, received {response!r}")


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
                greeting = read_until(fd, b"UART CMD ready\r\nType help for commands.\r\n> ", 10)
                run_command(fd, b"help", b"Commands: help, ping, led on, led off, status, echo TEXT\r\n")
                run_command(fd, b"ping", b"pong\r\n")
                # Windows terminals commonly send CRLF. LF must not create another prompt.
                os.write(fd, b"\n")
                if select.select([fd], [], [], 0.2)[0]:
                    raise AssertionError(f"extra output after CRLF: {os.read(fd, 4096)!r}")
                run_command(fd, b"led on", b"LED on\r\n")
                run_command(fd, b"status", b"LED: on\r\n")
                run_command(fd, b"echo hello", b"hello\r\n")
                run_command(fd, b"led off", b"LED off\r\n")
                run_command(fd, b"status", b"LED: off\r\n")
                run_command(fd, b"nope", b"Unknown command. Type help.\r\n")
                for value in b"pinh":
                    char = bytes([value])
                    os.write(fd, char)
                    if read_until(fd, char, 5) != char:
                        raise AssertionError("input echo failed during line edit")
                os.write(fd, b"\x7f")
                if read_until(fd, b"\b \b", 5) != b"\b \b":
                    raise AssertionError("backspace display failed")
                os.write(fd, b"g")
                if read_until(fd, b"g", 5) != b"g":
                    raise AssertionError("input echo failed after backspace")
                os.write(fd, b"\r")
                if read_until(fd, b"> ", 5) != b"\r\npong\r\n> ":
                    raise AssertionError("backspace did not edit the command")

                for _ in range(48):
                    os.write(fd, b"x")
                    if read_until(fd, b"x", 5) != b"x":
                        raise AssertionError("input echo failed before the line limit")
                os.write(fd, b"y\r")
                if read_until(fd, b"> ", 5) != b"\r\nLine too long.\r\n> ":
                    raise AssertionError("overlong command was not rejected")

                print(f"Renode UART CMD: {greeting!r}; commands, CRLF and line editing passed")
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
