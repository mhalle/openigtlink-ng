/**
 * Transport-layer exception hierarchy.
 *
 * Parallels the C++ `oigtl::transport::*Error` family in
 * `core-cpp/include/oigtl/transport/errors.hpp` and the Python
 * `oigtl.net.errors` module. The codec-layer `ProtocolError`
 * hierarchy (in `runtime/errors.ts`) stays separate — a malformed
 * body surfaces as `ProtocolError` through a receive, while a
 * peer-side FIN surfaces as `ConnectionClosedError`. Callers can
 * distinguish.
 */

/** Base for every transport-layer failure. */
export class TransportError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "TransportError";
    Object.setPrototypeOf(this, new.target.prototype);
  }
}

/**
 * The peer closed the connection (FIN) or the local side called
 * `close()`. Any pending `receive()` resolves with this.
 */
export class ConnectionClosedError extends TransportError {
  constructor(message: string = "connection closed") {
    super(message);
    this.name = "ConnectionClosedError";
    Object.setPrototypeOf(this, ConnectionClosedError.prototype);
  }
}

/** A `receive()` / `send()` exceeded its wall-clock budget. */
export class TransportTimeoutError extends TransportError {
  constructor(message: string = "operation timed out") {
    super(message);
    this.name = "TransportTimeoutError";
    Object.setPrototypeOf(this, TransportTimeoutError.prototype);
  }
}

/**
 * Framer rejected the bytes on the wire. Reserved for framer-
 * specific issues (e.g., body_size exceeding the configured
 * `maxMessageSize`). Codec-layer errors (bad CRC, malformed
 * header) keep their `ProtocolError` types.
 */
export class FramingError extends TransportError {
  constructor(message: string) {
    super(message);
    this.name = "FramingError";
    Object.setPrototypeOf(this, FramingError.prototype);
  }
}
