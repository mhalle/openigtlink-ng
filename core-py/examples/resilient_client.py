# /// script
# requires-python = ">=3.11"
# dependencies = ["oigtl"]
# ///
"""Demonstration of ``oigtl.net.Client`` resilience features.

Ports ``core-cpp/examples/resilient_client.cpp`` to Python. Runs
server + client in one process so the demo is self-contained. We
deliberately tear the server down mid-session to force
auto-reconnect, then restart on the same port so buffered
messages drain.

Run::

    uv run --project core-py python core-py/examples/resilient_client.py

Expected output (details may vary by timing):

    [client] connecting to 127.0.0.1:<port> ...
    [client] Phase 1: 3 messages while connected
      [server] TRANSFORM from demo-tracker: [0.00, 0.00, 0.00]
      [server] TRANSFORM from demo-tracker: [1.00, 2.00, 3.00]
      [server] TRANSFORM from demo-tracker: [2.00, 4.00, 6.00]
    [client] Phase 2: stopping server to induce drop
    [client] on_disconnected
    [client] Phase 2: 5 messages during outage
    [client] reconnect attempt 1 failed; next in ~200 ms
    [client] Phase 3: restart server
      [server] TRANSFORM from demo-tracker: [3.00, 6.00, 9.00]
      ...
    [client] on_connected (#2)
    [client] Phase 3: 2 post-reconnect messages
      [server] TRANSFORM from demo-tracker: [8.00, 16.00, 24.00]
      [server] TRANSFORM from demo-tracker: [9.00, 18.00, 27.00]

    SUMMARY:
      server session 1 received: 3 messages
      server session 2 received: 7 messages (5 buffered + 2 live)
      disconnects observed:      1
      reconnects observed:       1
    PASS
"""

from __future__ import annotations

import asyncio
import sys
from datetime import timedelta

from oigtl.messages import Transform
from oigtl.net import Client, ClientOptions, OfflineOverflow, Server


async def _run_server_for(port: int, counter: list[int]) -> Server:
    """Boot a server on *port*, bump *counter* for every TRANSFORM."""
    server = await Server.listen(port)

    @server.on(Transform)
    async def _(env, peer):
        counter[0] += 1
        tx = env.body.matrix[9:12]
        print(
            f"  [server] TRANSFORM from {env.header.device_name}: "
            f"[{tx[0]:.2f}, {tx[1]:.2f}, {tx[2]:.2f}]"
        )

    return server


def _tf(i: int) -> Transform:
    return Transform(matrix=[1, 0, 0, 0, 1, 0, 0, 0, 1,
                             float(i), float(i * 2), float(i * 3)])


async def main() -> int:
    rx1 = [0]
    rx2 = [0]

    # ---- Phase 1: boot initial server -------------------------------
    s1 = await _run_server_for(0, rx1)
    port = s1.port
    s1_task = asyncio.create_task(s1.serve())

    # ---- Configure resilient client ---------------------------------
    opt = ClientOptions(
        auto_reconnect=True,
        tcp_keepalive=True,
        offline_buffer_capacity=20,
        offline_overflow_policy=OfflineOverflow.DROP_OLDEST,
        reconnect_initial_backoff=timedelta(milliseconds=100),
        reconnect_max_backoff=timedelta(milliseconds=500),
        default_device="demo-tracker",
    )

    print(f"[client] connecting to 127.0.0.1:{port} ...")
    client = await Client.connect("127.0.0.1", port, opt)

    reconnects = 0
    drops = 0

    @client.on_connected
    def _():
        nonlocal reconnects
        reconnects += 1
        print(f"[client] on_connected (#{reconnects})")

    @client.on_disconnected
    def _(cause):
        nonlocal drops
        drops += 1
        print("[client] on_disconnected")

    @client.on_reconnect_failed
    def _(attempt, delay):
        ms = int(delay.total_seconds() * 1000)
        print(
            f"[client] reconnect attempt {attempt} failed; "
            f"next in ~{ms} ms"
        )

    # ---- Phase 1: happy path ---------------------------------------
    print("[client] Phase 1: 3 messages while connected")
    for i in range(3):
        await client.send(_tf(i))
        await asyncio.sleep(0.05)
    await asyncio.sleep(0.15)

    # ---- Phase 2: induce drop --------------------------------------
    print("[client] Phase 2: stopping server to induce drop")
    await s1.close()
    await s1_task

    print("[client] Phase 2: 5 messages during outage")
    for i in range(3, 8):
        try:
            await client.send(_tf(i))
        except Exception as e:
            print(f"[client] send {i} threw: {e}")
        await asyncio.sleep(0.05)

    # ---- Phase 3: restart server on same port ----------------------
    print(f"[client] Phase 3: restart server on port {port}")
    s2 = await _run_server_for(port, rx2)
    s2_task = asyncio.create_task(s2.serve())
    await asyncio.sleep(1.0)    # reconnect window

    print("[client] Phase 3: 2 post-reconnect messages")
    for i in range(8, 10):
        await client.send(_tf(i))
        await asyncio.sleep(0.1)
    await asyncio.sleep(0.3)

    # ---- Cleanup ---------------------------------------------------
    await client.close()
    await s2.close()
    await s2_task

    print(f"\nSUMMARY:")
    print(f"  server session 1 received: {rx1[0]} messages")
    print(f"  server session 2 received: {rx2[0]} messages")
    print(f"  disconnects observed:      {drops}")
    print(f"  reconnects observed:       {reconnects}")

    ok = (rx1[0] >= 3 and rx2[0] >= 2 and drops >= 1 and reconnects >= 1)
    print(f"\n{'PASS' if ok else 'FAIL (unexpected counts)'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
