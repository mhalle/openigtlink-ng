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
#ifndef OIGTL_CLIENT_HPP
#define OIGTL_CLIENT_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
};

class Client {
 public:
    // Blocking connect with retry. Throws
    // `transport::ConnectionClosedError` after all retries fail.
    static Client connect(std::string host, std::uint16_t port,
                          ClientOptions opt = {});

    // Wrap an already-connected Connection (e.g. coming out of a
    // Noise wrapper, or from a Server::accept()).
    static Client adopt(std::unique_ptr<transport::Connection> conn,
                        ClientOptions opt = {});

    // Move-only.
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;
    ~Client();

    // ----- typed send -----

    // Convenience: fill device_name + timestamp from options, use
    // a default-constructed Envelope otherwise.
    template <class Msg>
    void send(const Msg& body) {
        Envelope<Msg> env;
        env.version = opt_.default_version;
        env.device_name = opt_.default_device;
        env.body = body;
        send_envelope(env);
    }

    // Convenience: caller supplies device_name.
    template <class Msg>
    void send(std::string device_name, const Msg& body) {
        Envelope<Msg> env;
        env.version = opt_.default_version;
        env.device_name = std::move(device_name);
        env.body = body;
        send_envelope(env);
    }

    // Convenience: device_name + explicit timestamp.
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

    // Full-control: caller owns the entire Envelope (metadata,
    // message_id, version).
    template <class Msg>
    void send(const Envelope<Msg>& env) { send_envelope(env); }

    // ----- typed receive -----

    // Block until a message matching `Msg::kTypeId` arrives; throw
    // `MessageTypeMismatch` if something else comes first (with
    // the received type_id on the exception so the caller can
    // log and decide).
    template <class Msg>
    Envelope<Msg> receive() {
        auto inc = receive_any();
        return unpack<Msg>(inc);
    }

    // Generic receive — returns the full transport::Incoming so
    // the caller can inspect header.type_id and dispatch.
    transport::Incoming receive_any();

    // ----- request/response -----

    // Send `req`, then block until a message with type_id
    // `Reply::kTypeId` arrives, or `timeout` elapses. Messages of
    // other types received in the meantime are either dispatched
    // through registered `on<T>` handlers (if any) or dropped.
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
            h(unpack<Msg>(inc));
        };
        dispatch_[Msg::kTypeId] = std::move(wrapper);
        return *this;
    }

    Client& on_unknown(
        std::function<void(const transport::Incoming&)> handler);
    Client& on_error(std::function<void(std::exception_ptr)> handler);

    // Blocking dispatch loop. Returns when:
    //   - `stop()` is called (from any thread),
    //   - the peer disconnects,
    //   - an `on_error` handler rethrows.
    void run();

    // Thread-safe. Signals `run()` to exit at the next loop turn.
    // Issues a `connection().close()` so any in-flight receive
    // unblocks promptly.
    void stop();

    // ----- escape hatches -----

    transport::Connection& connection() { return *conn_; }
    const ClientOptions& options() const { return opt_; }
    void close();

 private:
    Client() = default;

    template <class Msg>
    void send_envelope(const Envelope<Msg>& env);

    template <class Reply>
    Envelope<Reply> wait_for_typed(std::chrono::milliseconds timeout);

    std::unique_ptr<transport::Connection> conn_;
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
    auto wire = pack(env);
    // Direct-write fast path — avoids the per-call hop through the
    // io_context thread. 10× faster than `send().get()` on
    // small-frame streams; ergonomics of the facade don't change.
    // The send_timeout option is a documented no-op on the fast
    // path today; the kernel write is either immediate or the
    // connection is dead.
    conn_->send_sync(wire);
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
        auto fut = conn_->receive();
        const auto chunk = std::min<std::chrono::milliseconds>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                remaining),
            std::chrono::milliseconds(250));
        if (!fut.wait_for(chunk)) {
            fut.cancel();
            continue;
        }
        auto inc = fut.get();
        if (inc.header.type_id == Reply::kTypeId) {
            return unpack<Reply>(inc);
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
