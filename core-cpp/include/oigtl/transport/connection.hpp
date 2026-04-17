// Connection — the stateful, capability-bearing transport object.
//
// Implementations: loopback (tests), TCP (Phase 2), TLS wrapper
// (Phase 3). All honor the five contract properties in
// TRANSPORT_PLAN.md:
//   1. stateful + capability-bearing  (capability(), peer_*)
//   2. opaque message envelope        (receive() yields Incoming)
//   3. pluggable framer               (chosen at construction)
//   4. async / future-based           (every op returns Future<T>)
//   5. TLS as a wrapper               (transport::tls::wrap, later)
#ifndef OIGTL_TRANSPORT_CONNECTION_HPP
#define OIGTL_TRANSPORT_CONNECTION_HPP

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "oigtl/transport/framer.hpp"
#include "oigtl/transport/future.hpp"

namespace oigtl::transport {

class Connection {
 public:
    // ----- Property 1: stateful, capability-bearing -----
    // Read-only after negotiation. Populated by the implementation
    // before this Connection is handed out.
    virtual std::optional<std::string>
    capability(std::string_view key) const = 0;

    virtual std::string peer_address() const = 0;
    virtual std::uint16_t peer_port() const = 0;

    // Protocol version negotiated on this connection, or 0 if
    // negotiation hasn't happened (or isn't meaningful, e.g.
    // loopback).
    virtual std::uint16_t negotiated_version() const = 0;

    // ----- Property 4: async receive / send / close -----
    // receive() resolves with the next framed message. Ordering:
    // sequential in sends-from-peer order.
    virtual Future<Incoming> receive() = 0;

    // send() takes a fully-packed wire message (58-byte header
    // followed by body_size bytes). The framer wraps for the wire
    // (v3: identity copy). Resolves when the bytes have been handed
    // to the OS / peer inbox.
    virtual Future<void> send(const std::uint8_t* wire,
                              std::size_t length) = 0;

    // Convenience overload for contiguous containers.
    Future<void> send(const std::vector<std::uint8_t>& wire) {
        return send(wire.data(), wire.size());
    }

    // Blocking write on the caller's thread — no io_context hop.
    // Throws `ConnectionClosedError` on failure. Safe to call
    // concurrently with an outstanding async `receive()`
    // (TCP is full-duplex at the kernel level); NOT safe to call
    // concurrently with itself from multiple threads unless the
    // implementation documents otherwise. The TCP backend
    // serialises concurrent send_sync callers with an internal
    // mutex, so in practice it is safe from multiple threads too.
    //
    // This is the fast path the ergonomic Client uses — round-trip
    // through an executor per send is the kind of overhead that
    // shows up at 50k+ msg/s.
    virtual void send_sync(const std::uint8_t* wire,
                           std::size_t length) = 0;

    void send_sync(const std::vector<std::uint8_t>& wire) {
        send_sync(wire.data(), wire.size());
    }

    // Blocking receive on the caller's thread — no io_context hop.
    // `timeout < 0` means block indefinitely.
    //
    // Throws `TimeoutError` on deadline expiry,
    // `ConnectionClosedError` on peer close / error, or whatever
    // the framer throws on malformed bytes.
    //
    // Mixing `receive()` (async) and `receive_sync()` on the same
    // Connection is undefined — they'd race on the inbox. Pick one
    // mode per Connection. The ergonomic Client uses
    // receive_sync exclusively.
    virtual Incoming
    receive_sync(std::chrono::milliseconds timeout) = 0;

    Incoming receive_sync() {
        return receive_sync(std::chrono::milliseconds(-1));
    }

    // Graceful close. Pending receive() resolves with
    // ConnectionClosedError. Idempotent.
    virtual Future<void> close() = 0;

    virtual ~Connection() = default;
};

// Server-side accept primitive. Each `accept()` resolves with the
// next inbound connection (or a transport error). An iterator
// adapter (`begin()`/`end()`) can layer on top; the Future-returning
// accept is the primitive because it composes with `when_any(accept,
// shutdown)` for graceful server exit.
class Acceptor {
 public:
    virtual Future<std::unique_ptr<Connection>> accept() = 0;
    virtual std::uint16_t local_port() const = 0;
    // Stop accepting. Pending accept() resolves with
    // OperationCancelledError.
    virtual Future<void> close() = 0;
    virtual ~Acceptor() = default;
};

// Paired loopback connections. send() on one appears in receive()
// on the other. Uses `make_v3_framer()` on both sides. Intended for
// tests and examples — no real I/O, no thread hop.
//
// Returned pair is (a, b); bytes written to a.send() emerge from
// b.receive(), and vice versa.
std::pair<std::unique_ptr<Connection>, std::unique_ptr<Connection>>
make_loopback_pair();

}  // namespace oigtl::transport

#endif  // OIGTL_TRANSPORT_CONNECTION_HPP
