"""Synchronous wrapper over :class:`~oigtl.net.client.Client`.

For research scripts that don't want to learn asyncio. Same
capability surface (connect / send / receive / receive_any /
dispatch / close), wrapped with blocking semantics by submitting
coroutines to a shared background event loop.

    from oigtl.net import SyncClient

    c = SyncClient.connect("tracker.lab", 18944)
    c.send(Transform(matrix=[...]))
    env = c.receive(Status)
    print(env.body.status_message)

All the same error types raise. Timeouts are wall-clock-honest:
``c.receive(Status, timeout=5)`` returns in at most 5 seconds
regardless of how many unrelated messages arrive in between.

The sync client is also importable as ``Client.connect_sync``:

    from oigtl.net import Client

    c = Client.connect_sync("tracker.lab", 18944)
"""

from __future__ import annotations

from datetime import timedelta
from typing import Any, Iterator, TypeVar

from pydantic import BaseModel

from oigtl.net._event_loop import submit
from oigtl.net._options import ClientOptions, Envelope
from oigtl.net.client import Client as _AsyncClient
from oigtl.net.errors import ConnectionClosedError

__all__ = ["SyncClient"]


M = TypeVar("M", bound=BaseModel)


class SyncClient:
    """Blocking façade over :class:`oigtl.net.client.Client`.

    Methods here mirror the async client's names; each dispatches
    through the shared background loop and blocks for the result.
    Thread-safe — multiple producer threads can call ``send()``
    concurrently; the underlying async client's send lock
    serialises writes.
    """

    # --------------------------------------------------------------
    # Construction
    # --------------------------------------------------------------

    def __init__(self, inner: _AsyncClient) -> None:
        # Private — callers use SyncClient.connect() for symmetry
        # with the async API.
        self._inner = inner

    @classmethod
    def connect(
        cls,
        host: str,
        port: int,
        options: ClientOptions | None = None,
    ) -> "SyncClient":
        """Open a TCP connection; block until ready.

        Same semantics as :meth:`Client.connect` but returns a sync
        handle. Raises the same errors
        (:class:`~oigtl.net.errors.ConnectionClosedError`,
        :class:`~oigtl.net.errors.TimeoutError`).
        """
        inner = submit(_AsyncClient.connect(host, port, options)).result()
        return cls(inner)

    # --------------------------------------------------------------
    # Properties
    # --------------------------------------------------------------

    @property
    def options(self) -> ClientOptions:
        return self._inner.options

    @property
    def peer(self) -> tuple[str, int] | None:
        return self._inner.peer

    # --------------------------------------------------------------
    # I/O
    # --------------------------------------------------------------

    def send(
        self,
        message: BaseModel,
        *,
        device_name: str | None = None,
        timestamp: int = 0,
    ) -> None:
        """Send *message* synchronously."""
        submit(self._inner.send(
            message,
            device_name=device_name,
            timestamp=timestamp,
        )).result()

    def receive(
        self,
        message_type: type[M],
        *,
        timeout: timedelta | float | int | None = None,
        timeout_ms: float | int | None = None,
    ) -> Envelope[M]:
        """Block until a message of *message_type* arrives.

        Pass ``timeout=<seconds>`` or ``timeout_ms=<ms>``. Setting
        both raises ``ValueError``.
        """
        return submit(self._inner.receive(
            message_type, timeout=timeout, timeout_ms=timeout_ms,
        )).result()

    def receive_any(
        self,
        *,
        timeout: timedelta | float | int | None = None,
        timeout_ms: float | int | None = None,
    ) -> Envelope[BaseModel]:
        """Block until the next message of any type arrives.

        Pass ``timeout=<seconds>`` or ``timeout_ms=<ms>``. Setting
        both raises ``ValueError``.
        """
        return submit(self._inner.receive_any(
            timeout=timeout, timeout_ms=timeout_ms,
        )).result()

    def messages(
        self,
        *,
        timeout: timedelta | float | int | None = None,
        timeout_ms: float | int | None = None,
    ) -> Iterator[Envelope[BaseModel]]:
        """Iterator of incoming messages.

        Each ``next()`` blocks until a message arrives or the per-
        message ``timeout`` / ``timeout_ms`` elapses. Yielding ends
        when the peer closes or :meth:`close` is called.

        Translates the async ``async for msg in c.messages():`` idiom
        to::

            for msg in c.messages():
                handle(msg)
        """
        while True:
            try:
                env = self.receive_any(
                    timeout=timeout, timeout_ms=timeout_ms,
                )
            except ConnectionClosedError:
                return
            yield env

    # --------------------------------------------------------------
    # Dispatch
    # --------------------------------------------------------------

    def on(self, message_type: type[M]):
        """Delegate to the async client's decorator-style register.

        The handler is still an async function; the sync wrapper
        doesn't try to paper over that — mixing sync handlers into
        an event loop is a footgun. Use :meth:`messages` + a sync
        ``for`` loop if you want blocking handler semantics.
        """
        return self._inner.on(message_type)

    def on_unknown(self, handler):
        return self._inner.on_unknown(handler)

    def on_error(self, handler):
        return self._inner.on_error(handler)

    def run(self) -> None:
        """Block until the peer closes or :meth:`close` is called.

        Dispatch loop runs on the background event loop; this
        method just waits for it.
        """
        submit(self._inner.run()).result()

    # --------------------------------------------------------------
    # Teardown
    # --------------------------------------------------------------

    def close(self) -> None:
        """Close the underlying connection."""
        try:
            submit(self._inner.close()).result(timeout=5)
        except Exception:
            # Idempotent close semantics; swallow so __exit__ doesn't
            # mask the real error.
            pass

    def __enter__(self) -> "SyncClient":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()
