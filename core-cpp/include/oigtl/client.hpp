// oigtl::Client — the one-call SDK for OpenIGTLink traffic.
//
// The goal: the shortest path from "I want to talk OpenIGTLink"
// to working code. Under the hood it's just `transport::Connection`
// + `pack`/`unpack`; this facade adds typed send/receive,
// timestamp and device-name defaults, retry-on-connect, a
// dispatch loop for callback-style code, and request/response
// pairing.
//
// Blocking by design — the async `transport::Connection` layer is
// still there for users who want it (`client.connection()` returns
// a reference). New code that prefers async can use that directly.
//
// Resilience (opt-in via ClientOptions):
//   - `auto_reconnect` — background reconnect on connection drop
//     with exponential backoff + jitter
//   - `tcp_keepalive`  — enable SO_KEEPALIVE with tuned intervals
//   - `offline_buffer_capacity` — queue messages sent while
//     disconnected and drain on reconnect
//   See core-cpp/CLIENT_TRANSPORT_PLAN.md for the design rationale.
//
// Usage, minimum viable:
//     auto c = oigtl::Client::connect("host", 18944);
//     c.send(oigtl::messages::Transform{ .matrix = {...} });
//     auto env = c.receive<oigtl::messages::Status>();
//
// Usage, callback-dispatch:
//     oigtl::Client::connect("host", 18944)
//         .on<oigtl::messages::Transform>([&](auto& env) { ... })
//         .on<oigtl::messages::Image    >([&](auto& env) { ... })
//         .on_error([&](auto eptr) { ... })
//         .run();
//
// Usage, resilient:
//     oigtl::ClientOptions opt;
//     opt.auto_reconnect = true;
//     opt.tcp_keepalive = true;
//     opt.offline_buffer_capacity = 100;
//     auto c = oigtl::Client::connect("host", 18944, opt);
//     c.on_disconnected([](auto) { /* log */ });
//     c.on_connected([] { /* resubscribe */ });
//     while (running) { c.send(tracker.latest()); }    // buffers
//                                                      // across drops
#ifndef OIGTL_CLIENT_HPP
#define OIGTL_CLIENT_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "oigtl/envelope.hpp"
#include "oigtl/pack.hpp"
#include "oigtl/transport/connection.hpp"

namespace oigtl {

struct ClientOptions {
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::milliseconds send_timeout{1000};
    // Default receive timeout is "wait indefinitely" — the common
    // dispatch-loop case is "block in receive until something
    // arrives or the peer disconnects". Override for poll-style
    // use.
    std::chrono::milliseconds receive_timeout{
        std::chrono::hours(24 * 365)};
    int connect_retries = 3;
    std::chrono::milliseconds retry_backoff{500};

    // Fills `Envelope.device_name` when the caller omits it.
    std::string default_device = "oigtl-client";

    // Fills `Envelope.version`. 1 = legacy/no ext-hdr, 2 = v2
    // (extended header + metadata), 3 = v3. Default 2 is what
    // most modern peers expect and what upstream emits by default.
    std::uint16_t default_version = 2;

    // ------- Resilience (opt-in) ---------------------------------

    // When true, a connection drop triggers a background reconnect
    // attempt. send() and receive<T>() buffer / block rather than
    // throwing ConnectionClosedError immediately. Default false
    // for back-compat with pre-v0.3 Client behavior.
    bool auto_reconnect = false;

    // Exponential-backoff parameters for auto-reconnect. Initial
    // delay doubles after each consecutive failure up to the cap,
    // with ± jitter applied each round.
    std::chrono::milliseconds reconnect_initial_backoff{200};
    std::chrono::milliseconds reconnect_max_backoff{30'000};
    double reconnect_backoff_jitter = 0.25;

    // 0 = reconnect indefinitely. Positive = give up after this
    // many consecutive failures; on_reconnect_failed() fires and
    // subsequent send/receive throw ConnectionClosedError
    // terminally.
    int reconnect_max_attempts = 0;

    // When true, enable SO_KEEPALIVE on the underlying socket. The
    // three follow-up knobs tune the idle / probe interval / probe
    // count. Per-platform mapping lives in detail::net_compat.
    bool tcp_keepalive = false;
    std::chrono::seconds tcp_keepalive_idle{30};
    std::chrono::seconds tcp_keepalive_interval{10};
    int tcp_keepalive_count = 3;

    // 0 = disabled (send() throws when disconnected; existing
    // behavior). Positive = at most this many queued messages
    // while the connection is down. On reconnect the queue drains
    // in FIFO order before new sends.
    std::size_t offline_buffer_capacity = 0;

    // Overflow policy when the offline buffer is full. Consulted
    // only when offline_buffer_capacity > 0 and the send() occurs
    // while disconnected.
    enum class OfflineOverflow {
        DropOldest,    // discard the queue head; append the new send
        DropNewest,    // keep the queue; throw BufferOverflowError
        Block,         // wait up to send_timeout for space
    };
    OfflineOverflow offline_overflow_policy =
        OfflineOverflow::DropNewest;
};

class Client {
 public:
    // Blocking connect with retry. Throws
    // `transport::ConnectionClosedError` after all retries fail.
    // If opt.auto_reconnect is true and reconnect_max_attempts is
    // 0, connect() returns after the FIRST successful connection
    // but the background worker will continue attempts
    // indefinitely on subsequent drops.
    static Client connect(std::string host, std::uint16_t port,
                          ClientOptions opt = {});

    // Wrap an already-connected Connection (e.g. coming out of a
    // Noise wrapper, or from a Server::accept()). auto_reconnect
    // is NOT useful here — we don't know how to reconnect an
    // externally-owned socket. Set to false implicitly.
    static Client adopt(std::unique_ptr<transport::Connection> conn,
                        ClientOptions opt = {});

    // Move-only. Move is cheap (shared_ptr of internal state);
    // copy is deleted.
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;
    ~Client();

    // ----- typed send -----

    template <class Msg>
    void send(const Msg& body) {
        Envelope<Msg> env;
        env.version = opt_.default_version;
        env.device_name = opt_.default_device;
        env.body = body;
        send_envelope(env);
    }

    template <class Msg>
    void send(std::string device_name, const Msg& body) {
        Envelope<Msg> env;
        env.version = opt_.default_version;
        env.device_name = std::move(device_name);
        env.body = body;
        send_envelope(env);
    }

    template <class Msg>
    void send(std::string device_name, const Msg& body,
              std::chrono::system_clock::time_point when) {
        Envelope<Msg> env;
        env.version = opt_.default_version;
        env.device_name = std::move(device_name);
        env.timestamp = to_igtl_timestamp(when);
        env.body = body;
        send_envelope(env);
    }

    template <class Msg>
    void send(const Envelope<Msg>& env) { send_envelope(env); }

    // ----- typed receive -----

    template <class Msg>
    Envelope<Msg> receive() {
        auto inc = receive_any();
        return unpack_envelope<Msg>(inc);
    }

    transport::Incoming receive_any();

    // ----- request/response -----

    template <class Reply, class Request>
    Envelope<Reply> request_response(
            const Request& req,
            std::chrono::milliseconds timeout =
                std::chrono::seconds(5)) {
        send(req);
        return wait_for_typed<Reply>(timeout);
    }

    template <class Reply, class Request>
    Envelope<Reply> request_response(
            const Envelope<Request>& req,
            std::chrono::milliseconds timeout =
                std::chrono::seconds(5)) {
        send(req);
        return wait_for_typed<Reply>(timeout);
    }

    // ----- callback dispatch -----

    template <class Msg, class F>
    Client& on(F handler) {
        auto wrapper = [h = std::move(handler)]
                       (const transport::Incoming& inc) {
            h(unpack_envelope<Msg>(inc));
        };
        dispatch_[Msg::kTypeId] = std::move(wrapper);
        return *this;
    }

    Client& on_unknown(
        std::function<void(const transport::Incoming&)> handler);
    Client& on_error(std::function<void(std::exception_ptr)> handler);

    // ----- lifecycle callbacks (only fire when auto_reconnect active) -----

    // Called on the reconnect worker thread after each successful
    // reconnect. Includes the initial connect IF it's followed by
    // one or more drop + reconnect cycles. The very first connect
    // via Client::connect() does NOT fire this — that's the
    // explicit-success return from the static factory.
    Client& on_connected(std::function<void()> h);

    // Called on the reconnect worker thread after the connection
    // drops. `cause` carries the exception_ptr that surfaced the
    // drop (std::nullptr_t if the drop was user-initiated via
    // close/stop). Passed by value so the callback may store it
    // for diagnostics; shared_ptr internally.
    Client& on_disconnected(std::function<void(std::exception_ptr)> h);

    // Called on the reconnect worker thread after each failed
    // reconnect attempt. `attempt` is 1-based. `next_delay` is
    // the jittered backoff before the next attempt, or zero when
    // the worker is giving up (max_attempts reached).
    Client& on_reconnect_failed(
        std::function<void(int attempt,
                           std::chrono::milliseconds next_delay)> h);

    // ----- dispatch loop -----

    void run();
    void stop();

    // ----- escape hatches -----

    // Returns the current Connection. With auto_reconnect active,
    // this pointer becomes invalid when the underlying connection
    // drops; the reference is safe only between send()/receive()
    // calls under the same lock you didn't take. Prefer the
    // send/receive API in resilient configurations.
    transport::Connection& connection();

    const ClientOptions& options() const { return opt_; }
    void close();

    // Opaque shared state; definition lives in client.cpp. Public
    // only because the reconnect worker function (in the .cpp's
    // anonymous namespace) needs to take a shared_ptr to it;
    // callers can't do anything with this type.
    struct State;

 private:
    Client() = default;

    template <class Msg>
    void send_envelope(const Envelope<Msg>& env);

    template <class Reply>
    Envelope<Reply> wait_for_typed(std::chrono::milliseconds timeout);

    // Non-template bottleneck for send: takes already-framed wire
    // bytes, routes to either the live connection or the offline
    // buffer (when auto_reconnect is active). Takes `state_->mu`
    // internally.
    void send_bytes(std::vector<std::uint8_t>&& wire);

    // Non-template bottleneck for receive: returns one
    // transport::Incoming. Coordinates with the reconnect worker
    // so a drop-then-reconnect is transparent to the caller.
    transport::Incoming receive_one(std::chrono::milliseconds timeout);

    std::shared_ptr<State> state_;
    ClientOptions opt_;

    using DispatchFn =
        std::function<void(const transport::Incoming&)>;
    std::unordered_map<std::string, DispatchFn> dispatch_;
    std::function<void(const transport::Incoming&)> on_unknown_;
    std::function<void(std::exception_ptr)> on_error_;
    std::atomic<bool> stop_requested_{false};
};

// ===========================================================================
// Template definitions
// ===========================================================================
template <class Msg>
void Client::send_envelope(const Envelope<Msg>& env) {
    auto wire = pack_envelope(env);
    send_bytes(std::move(wire));
}

template <class Reply>
Envelope<Reply> Client::wait_for_typed(
        std::chrono::milliseconds timeout) {
    const auto deadline =
        std::chrono::steady_clock::now() + timeout;
    while (true) {
        const auto remaining = deadline -
                               std::chrono::steady_clock::now();
        if (remaining <= std::chrono::milliseconds::zero()) {
            throw transport::TimeoutError{};
        }
        auto inc = receive_one(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                remaining));
        if (inc.header.type_id == Reply::kTypeId) {
            return unpack_envelope<Reply>(inc);
        }
        // Dispatch other types through on<T>, or drop.
        auto it = dispatch_.find(inc.header.type_id);
        if (it != dispatch_.end()) {
            try { it->second(inc); }
            catch (...) { if (on_error_) on_error_(std::current_exception()); }
        } else if (on_unknown_) {
            on_unknown_(inc);
        }
    }
}

}  // namespace oigtl

#endif  // OIGTL_CLIENT_HPP
