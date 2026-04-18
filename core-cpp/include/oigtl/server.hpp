// oigtl::Server — the mirror of Client, for peers that accept
// incoming connections (trackers serving data, receivers waiting
// for an imager, etc.).
//
// Two usage models:
//
// 1. Explicit accept:
//        auto s = oigtl::Server::listen(18944);
//        auto peer = s.accept();          // blocks; returns Client
//        peer.send(...);
//
// 2. Dispatch-loop:
//        oigtl::Server::listen(18944)
//            .on<Transform>([](auto& peer, auto& env) { ... })
//            .on_connected([](auto& peer) { ... })
//            .on_disconnected([](auto& peer) { ... })
//            .run();
//
// In dispatch-loop mode each accepted connection gets its own
// thread running a Client dispatch loop pre-configured with the
// server's registered handlers.
#ifndef OIGTL_SERVER_HPP
#define OIGTL_SERVER_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "oigtl/client.hpp"
#include "oigtl/envelope.hpp"
#include "oigtl/pack.hpp"
#include "oigtl/transport/connection.hpp"
#include "oigtl/transport/policy.hpp"

namespace oigtl {

struct ServerOptions {
    std::string bind_address = "0.0.0.0";

    // Used for both: the `accept()`-returned Client, and each
    // dispatch-loop-spawned Client.
    ClientOptions client_defaults;

    // In dispatch-loop mode, accepting more than this many
    // concurrent peers blocks the accept loop until one drops.
    std::size_t max_connections = 32;

    // Who may connect + per-peer resource caps. Default is "no
    // restriction, accept any peer, unlimited concurrency" —
    // matching the behaviour before this field existed. For
    // researcher-friendly presets see the builder methods on
    // `Server` (restrict_to_this_machine_only(), allow_peer(),
    // etc.) which mutate the active policy in-place.
    transport::PeerPolicy policy;
};

class Server {
 public:
    static Server listen(std::uint16_t port,
                         ServerOptions opt = {});

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) noexcept;
    Server& operator=(Server&&) noexcept;
    ~Server();

    // Block until one peer connects, return a Client owning that
    // connection. Applies `opt.client_defaults` to the returned
    // Client. Throws on shutdown or acceptor error.
    Client accept();

    // Register a typed handler for dispatch-loop mode. The first
    // argument is a reference to the per-peer Client so the
    // handler can send responses.
    template <class Msg, class F>
    Server& on(F handler) {
        installers_.push_back(
            [h = std::move(handler)](Client& c) {
                c.on<Msg>([&c, h](const Envelope<Msg>& env) {
                    h(c, env);
                });
            });
        return *this;
    }

    // Fallback for type_ids not registered via on<T>.
    Server& on_unknown(
        std::function<void(Client&, const transport::Incoming&)> h);

    // Invoked on the per-peer thread just after accept and before
    // the dispatch loop starts. Use to send a greeting, send
    // capability, etc.
    Server& on_connected(std::function<void(Client&)> h);

    // Invoked on the per-peer thread when the peer's dispatch
    // loop exits (peer closed, error, or server shutdown).
    Server& on_disconnected(std::function<void(Client&)> h);

    // Handler-error callback. Invoked on the peer's thread; can
    // inspect the exception and decide (log / rethrow / ignore).
    // If not set, exceptions tear down the per-peer dispatch loop.
    Server& on_error(
        std::function<void(Client&, std::exception_ptr)> h);

    // Blocking accept loop. Spawns a thread per accepted peer that
    // runs a Client dispatch loop with the registered handlers.
    // Returns when `stop()` is called.
    void run();

    // Thread-safe. Stops the accept loop and closes every
    // per-peer connection; their dispatch threads then exit and
    // are joined in this Server's destructor.
    void stop();

    // =============================================================
    // Who-may-connect builders. Safe to call before or after
    // `listen()`; changes to the policy apply to the next
    // accept() and do NOT drop existing peers.
    //
    // These are the researcher-friendly names; if you need to
    // manipulate the PeerPolicy struct directly (bulk changes,
    // custom IpRange ranges, etc.) use `set_policy()` instead.
    //
    // All builders return *this for chaining:
    //     Server::listen(18944)
    //         .restrict_to_local_subnet()
    //         .set_max_simultaneous_clients(4)
    //         .on<Transform>(...)
    //         .run();
    // =============================================================

    /// Loopback only — accept connections only from this machine.
    /// Good for dev work and never-on-network scenarios.
    Server& restrict_to_this_machine_only();

    /// Peers on the same IP subnet as one of this host's active
    /// interfaces. `iface_name` optional — limits to a single
    /// named interface (e.g. "eth0" on Linux, "Ethernet 2" on
    /// Windows); empty means "any active non-link-local interface".
    /// Loopback is included when no interface filter is given, so
    /// local tools keep working.
    Server& restrict_to_local_subnet();
    Server& restrict_to_local_subnet(const std::string& iface_name);

    /// Add a single peer IP, CIDR block, or hostname to the
    /// allow-list. Returns *this — check `policy().allowed_peers`
    /// after the call if you need to confirm a hostname resolved.
    Server& allow_peer(const std::string& ip_or_host);

    /// Add an inclusive IP range to the allow-list. Both endpoints
    /// must be the same family (IPv4 or IPv6).
    Server& allow_peer_range(const std::string& first_ip,
                             const std::string& last_ip);

    /// Cap how many peers may be connected at once. 0 = unlimited.
    /// Enforced at accept time: an over-cap peer is accepted then
    /// immediately closed.
    Server& set_max_simultaneous_clients(std::size_t n);

    /// Close a peer if no bytes have arrived for this long. 0 =
    /// no timeout. Useful for cleaning up dead TCP half-connections
    /// (device unplugged / cable yanked).
    Server& disconnect_if_silent_for(std::chrono::seconds t);

    /// Reject any incoming message larger than this many bytes
    /// before allocating the body. 0 = no cap. Protects against
    /// pathological peers sending bogus huge body_size headers.
    Server& set_max_message_size_bytes(std::size_t n);

    /// Replace the entire policy in one call. Prefer the named
    /// builders above for common cases.
    Server& set_policy(transport::PeerPolicy p);

    /// Snapshot of the current policy. Useful for tests and
    /// logging.
    const transport::PeerPolicy& policy() const { return opt_.policy; }

    std::uint16_t local_port() const;
    void close();

 private:
    Server() = default;
    void serve_peer(std::unique_ptr<transport::Connection> conn);

    std::unique_ptr<transport::Acceptor> acceptor_;
    ServerOptions opt_;

    std::vector<std::function<void(Client&)>> installers_;
    std::function<void(Client&, const transport::Incoming&)>
        on_unknown_;
    std::function<void(Client&)> on_connected_;
    std::function<void(Client&)> on_disconnected_;
    std::function<void(Client&, std::exception_ptr)> on_error_;

    std::atomic<bool> stop_requested_{false};
    std::mutex peers_mu_;
    std::vector<std::thread> peer_threads_;
};

}  // namespace oigtl

#endif  // OIGTL_SERVER_HPP
