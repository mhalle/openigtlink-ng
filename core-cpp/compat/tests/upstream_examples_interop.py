#!/usr/bin/env python3
# /// script
# requires-python = ">=3.9"
# dependencies = []
# ///
"""Live interop gate — portable replacement for the bash version.

Unmodified upstream ReceiveServer.cxx (linked only against our shim,
not upstream's libOpenIGTLink.a) reads TRANSFORM messages from
unmodified upstream TrackerClient.cxx (also via our shim).

Passes if ReceiveServer prints at least 5 "Receiving TRANSFORM"
lines within a 2-second window — i.e. the full stack works:
ClientSocket::ConnectToServer, Send, ServerSocket::WaitFor-
Connection, Socket::Receive (header then body), MessageHeader::
Unpack, TransformMessage dispatch.

Invoked by ctest with three positional args:
    argv[1] = path to upstream_ReceiveServer_via_shim
    argv[2] = path to upstream_TrackerClient_via_shim
    argv[3] = TCP port to use

Uses only the Python standard library so it runs unchanged on
Linux, macOS, and windows-latest CI without any extra install.
"""

from __future__ import annotations

import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def wait_for_listen(port: int, timeout_s: float = 2.0) -> bool:
    """Poll a TCP connect() to 127.0.0.1:port until it succeeds."""
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                return True
        except OSError:
            time.sleep(0.1)
    return False


def terminate(proc: subprocess.Popen) -> None:
    """Best-effort stop. Windows has no SIGTERM, so terminate() maps
    to TerminateProcess there; Popen.terminate() handles the
    abstraction for us.
    """
    if proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        proc.kill()
        try:
            proc.wait(timeout=1.0)
        except subprocess.TimeoutExpired:
            pass


def main(argv: list[str]) -> int:
    if len(argv) != 4:
        print(
            f"usage: {argv[0]} <server_exe> <client_exe> <port>",
            file=sys.stderr,
        )
        return 2

    server_exe = argv[1]
    client_exe = argv[2]
    port = int(argv[3])

    if not Path(server_exe).exists():
        print(f"FAIL: server binary not found: {server_exe}", file=sys.stderr)
        return 1
    if not Path(client_exe).exists():
        print(f"FAIL: client binary not found: {client_exe}", file=sys.stderr)
        return 1

    with tempfile.NamedTemporaryFile(
        mode="w+b", suffix=".log", delete=False
    ) as log:
        log_path = log.name

    try:
        # Start the server and point its stdout+stderr at the log.
        with open(log_path, "wb") as log_sink:
            server = subprocess.Popen(
                [server_exe, str(port)],
                stdout=log_sink,
                stderr=subprocess.STDOUT,
            )

            try:
                if not wait_for_listen(port):
                    server_log = Path(log_path).read_text(errors="replace")
                    print(
                        f"FAIL: server never bound on port {port}\n"
                        f"--- ReceiveServer output ---\n{server_log}",
                        file=sys.stderr,
                    )
                    return 1

                # TrackerClient runs forever; cap with a wait + kill.
                client = subprocess.Popen(
                    [client_exe, "127.0.0.1", str(port), "50"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                try:
                    time.sleep(2.0)
                finally:
                    terminate(client)

                # Small grace period so the server flushes its last
                # "Receiving TRANSFORM" line before we ask it to exit.
                time.sleep(0.3)
            finally:
                terminate(server)

        server_log = Path(log_path).read_text(errors="replace")
        count = sum(
            1 for line in server_log.splitlines()
            if "Receiving TRANSFORM" in line
        )
        if count < 5:
            print(
                f"FAIL: expected >=5 TRANSFORM msgs, got {count}\n"
                f"--- ReceiveServer output ---\n{server_log}",
                file=sys.stderr,
            )
            return 1

        print(f"OK: {count} TRANSFORM messages round-tripped end-to-end")
        return 0
    finally:
        try:
            Path(log_path).unlink()
        except OSError:
            pass


if __name__ == "__main__":
    sys.exit(main(sys.argv))
