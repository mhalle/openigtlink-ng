// TCP backend factories. Hides asio from the public API — the
// returned Connection / Acceptor types are the abstract ones from
// connection.hpp.
//
// Runtime: a single library-internal io_context on one background
// thread, started lazily on first call and joined at process exit.
// Exposing that executor is a non-goal (TRANSPORT_PLAN.md §Decisions
// #2) — we want freedom to swap backends later.
#ifndef OIGTL_TRANSPORT_TCP_HPP
#define OIGTL_TRANSPORT_TCP_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "oigtl/transport/connection.hpp"
#include "oigtl/transport/future.hpp"

namespace oigtl::transport::tcp {

// Initiate a client connection to host:port. Resolves with a
// connected Connection (v3 framer) or throws via the Future on
// DNS / connect failure (ConnectionClosedError with a detail
// message).
Future<std::unique_ptr<Connection>>
connect(std::string host, std::uint16_t port);

// Bind a listening socket on `port` (0 = ephemeral; query the
// resulting port with `Acceptor::local_port()`). Binds to loopback
// by default; pass an explicit address to bind elsewhere.
std::unique_ptr<Acceptor>
listen(std::uint16_t port, std::string bind_address = "127.0.0.1");

}  // namespace oigtl::transport::tcp

#endif  // OIGTL_TRANSPORT_TCP_HPP
