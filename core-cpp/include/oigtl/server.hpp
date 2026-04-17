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

namespace oigtl {

struct ServerOptions {
    std::string bind_address = "0.0.0.0";

    // Used for both: the `accept()`-returned Client, and each
    // dispatch-loop-spawned Client.
    ClientOptions client_defaults;

    // In dispatch-loop mode, accepting more than this many
    // concurrent peers blocks the accept loop until one drops.
    std::size_t max_connections = 32;
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
