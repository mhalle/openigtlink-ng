// Transport-layer exception hierarchy.
//
// All transport operations report failure through exceptions carried
// on the returned Future<T>. The codec layer has its own
// `oigtl::error::*` hierarchy for protocol-level errors; those
// propagate up through `receive()` unchanged when a framer encounters
// malformed bytes.
//
// Naming: `*Error` for terminal failures, no `Exception` suffix (to
// stay consistent with the codec layer).
#ifndef OIGTL_TRANSPORT_ERRORS_HPP
#define OIGTL_TRANSPORT_ERRORS_HPP

#include <stdexcept>

namespace oigtl::transport {

// Base for every exception thrown or delivered by transport code.
// Catching this alone won't catch codec errors delivered through a
// Future — those keep their `oigtl::error::*` type.
class TransportError : public std::runtime_error {
 public:
    using std::runtime_error::runtime_error;
};

// The peer closed the connection (FIN received) or the local side
// called `close()`. Any pending receive() resolves with this.
class ConnectionClosedError : public TransportError {
 public:
    ConnectionClosedError()
        : TransportError("connection closed") {}
    explicit ConnectionClosedError(const std::string& msg)
        : TransportError(msg) {}
};

// `Future<T>::cancel()` was called and the operation hadn't yet
// resolved. Also delivered when a Connection is closed while a
// receive() is pending.
class OperationCancelledError : public TransportError {
 public:
    OperationCancelledError()
        : TransportError("operation cancelled") {}
};

// `sync::block_on(fut, timeout)` exceeded its wall-clock budget.
class TimeoutError : public TransportError {
 public:
    TimeoutError()
        : TransportError("operation timed out") {}
};

// Framer rejected the bytes on the wire. The codec's
// `oigtl::error::ProtocolError` subtypes are the common case; this
// one is reserved for framer-specific issues (e.g., a future v4
// stream framer seeing an unexpected chunk type).
class FramingError : public TransportError {
 public:
    using TransportError::TransportError;
};

}  // namespace oigtl::transport

#endif  // OIGTL_TRANSPORT_ERRORS_HPP
