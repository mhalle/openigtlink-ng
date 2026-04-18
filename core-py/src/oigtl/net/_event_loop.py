"""Background asyncio event loop for the sync API wrappers.

The sync :class:`~oigtl.net.SyncClient` and
:class:`~oigtl.net.SyncServer` delegate all I/O to the real async
implementation by scheduling coroutines onto a shared loop running
in a daemon thread. One loop per process is enough; asyncio is
thread-safe through :func:`asyncio.run_coroutine_threadsafe`, and
every producer/consumer is isolated by its own Client/Server state.

The loop is lazily created on first use and torn down at
interpreter shutdown via an ``atexit`` hook. Researchers writing
``c = Client.connect_sync(...)`` followed by process exit without
an explicit ``c.close()`` get clean teardown.
"""

from __future__ import annotations

import asyncio
import atexit
import threading
from concurrent.futures import Future as _Future
from typing import Any, Coroutine, TypeVar

__all__ = ["submit", "shutdown"]

_T = TypeVar("_T")

_loop: asyncio.AbstractEventLoop | None = None
_thread: threading.Thread | None = None
_lock = threading.Lock()


def _ensure_running() -> asyncio.AbstractEventLoop:
    """Start the shared loop on first call; return the running loop.

    Double-checked locking so the hot path (loop already running) is
    a single load.
    """
    global _loop, _thread
    if _loop is not None and _loop.is_running():
        return _loop

    with _lock:
        if _loop is not None and _loop.is_running():
            return _loop

        ready = threading.Event()

        def run() -> None:
            global _loop
            _loop = asyncio.new_event_loop()
            asyncio.set_event_loop(_loop)
            ready.set()
            try:
                _loop.run_forever()
            finally:
                # Drain outstanding tasks so the interpreter doesn't
                # warn about pending callbacks at exit.
                pending = [t for t in asyncio.all_tasks(_loop)
                           if not t.done()]
                for t in pending:
                    t.cancel()
                if pending:
                    _loop.run_until_complete(
                        asyncio.gather(*pending, return_exceptions=True)
                    )
                _loop.close()

        _thread = threading.Thread(
            target=run,
            name="oigtl-net-loop",
            daemon=True,
        )
        _thread.start()
        ready.wait()
        assert _loop is not None
        return _loop


def submit(coro: Coroutine[Any, Any, _T]) -> _Future[_T]:
    """Schedule *coro* on the shared loop; return a concurrent Future.

    Callers block on ``.result()`` to get the value synchronously.
    Exceptions raised inside the coroutine propagate out of
    ``.result()`` unchanged — researchers keep familiar ``try: ...
    except ConnectionClosedError: ...`` semantics on the sync surface.
    """
    loop = _ensure_running()
    return asyncio.run_coroutine_threadsafe(coro, loop)


def shutdown() -> None:
    """Tear down the shared loop.

    Registered via :mod:`atexit`. Idempotent. Safe to call from a
    test fixture that wants a fresh loop between tests, though in
    practice one loop-per-process is the intended model.
    """
    global _loop, _thread
    with _lock:
        if _loop is None or not _loop.is_running():
            _loop = None
            _thread = None
            return
        _loop.call_soon_threadsafe(_loop.stop)
    if _thread is not None:
        _thread.join(timeout=5)
    _loop = None
    _thread = None


atexit.register(shutdown)
