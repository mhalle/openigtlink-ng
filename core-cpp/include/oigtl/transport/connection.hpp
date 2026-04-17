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

    // Graceful close. Pending receive() resolves with
    // ConnectionClosedError. Idempotent.
    virtual Future<void> close() = 0;

    virtual ~Connection() = default;
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
