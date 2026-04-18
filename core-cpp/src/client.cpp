// Client — non-template implementation details.
//
// The old (pre-auto_reconnect) shape was a thin wrapper over a
// `std::unique_ptr<Connection>`. Now the connection + offline
// queue + backoff state all live inside a `Client::State` that's
// shared (via `shared_ptr`) between the user's Client handle and
// the optional reconnect worker thread. This lets the worker
// outlive a moved-from Client without ownership confusion, and
// lets the lock protecting the live connection sit in exactly one
// place.
//
// All user-facing send/receive calls route through `send_bytes()`
// and `receive_one()` — the two non-template bottlenecks that
// acquire `State::mu`, decide between the live path and the
// buffered / blocked path, and coordinate with the worker.

#include "oigtl/client.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <thread>

#include "oigtl/transport/detail/net_compat.hpp"
#include "oigtl/transport/errors.hpp"
#include "oigtl/transport/future.hpp"
#include "oigtl/transport/tcp.hpp"

namespace oigtl {

// ---------------------------------------------------------------
// State: mutable per-Client data the user thread + reconnect
// worker both touch.
// ---------------------------------------------------------------

struct Client::State {
    std::mutex mu;
    std::condition_variable cv;     // broadcast on connect/disconnect/terminate

    // Current live connection, or nullptr if disconnected. Set to
    // nullptr by the user thread (send/receive that caught
    // ConnectionClosedError) OR by an explicit close(); the
    // worker swaps in a new non-null connection after a
    // successful reconnect.
    std::unique_ptr<transport::Connection> conn;

    // Reconnect identity — captured at connect() time and kept
    // for the life of the Client. DNS is re-resolved on every
    // reconnect attempt via transport::tcp::connect().
    std::string host;
    std::uint16_t port = 0;

    // Copy of ClientOptions for worker access (avoids the worker
    // needing a pointer back to the Client handle).
    ClientOptions opt;

    // Offline buffer. Holds already-framed wire bytes keyed to
    // the packing done by pack(Envelope<T>); the worker drains
    // the queue on reconnect without re-traversing the typed
    // layer.
    std::deque<std::vector<std::uint8_t>> offline_queue;

    // Reconnect state machine.
    int  consecutive_failures = 0;
    bool terminal_failure = false;   // max_attempts reached
    bool stop_requested = false;     // set by close()/dtor

    // Lifecycle callbacks (fire only when auto_reconnect is on).
    std::function<void()> on_connected;
    std::function<void(std::exception_ptr)> on_disconnected;
    std::function<void(int, std::chrono::milliseconds)>
        on_reconnect_failed;

    // Worker thread. Non-joinable unless opt.auto_reconnect is on.
    std::thread worker;
};

namespace {

// Best-effort keepalive plumbing. `set_socket_fd` is
// implemented by connection_tcp.cpp to expose the native
// descriptor; if that API isn't available on every
// Connection impl, configure_keepalive is just a no-op.
// Today's sole impl (the asio-based one) does expose it.
void maybe_configure_keepalive(transport::Connection& conn,
                               const ClientOptions& opt) {
    if (!opt.tcp_keepalive) return;
    const auto fd = conn.native_socket();
    if (fd == transport::detail::invalid_socket) return;
    transport::detail::configure_keepalive(
        fd,
        opt.tcp_keepalive_idle,
        opt.tcp_keepalive_interval,
        opt.tcp_keepalive_count);
}

// Exponential backoff with ± jitter. `attempt` is 1-based.
std::chrono::milliseconds compute_backoff(
        int attempt, const ClientOptions& opt) {
    using ms = std::chrono::milliseconds;
    const auto base = opt.reconnect_initial_backoff.count();
    const auto cap  = opt.reconnect_max_backoff.count();
    // 2^(attempt - 1), saturating. attempt >= 32 → 2^31 overflows
    // int64 long before we'd care; clamp the shift.
    const int shift = std::min(attempt - 1, 30);
    std::int64_t raw = base * (static_cast<std::int64_t>(1) << shift);
    if (raw > cap) raw = cap;

    if (opt.reconnect_backoff_jitter > 0.0) {
        thread_local std::mt19937 rng{std::random_device{}()};
        const double j = opt.reconnect_backoff_jitter;
        std::uniform_real_distribution<double> dist(1.0 - j, 1.0 + j);
        raw = static_cast<std::int64_t>(raw * dist(rng));
        if (raw < 0) raw = 0;
        if (raw > cap) raw = cap;
    }
    return ms(raw);
}

// The reconnect worker loop. Waits for the `conn` field to go
// null (a drop), then runs a backoff loop attempting to
// re-establish. On success, swaps in the new connection and
// drains any buffered messages. On exhaustion (max_attempts),
// marks terminal_failure and wakes all waiters so blocked
// send/receive calls can propagate the error.
void worker_loop(std::shared_ptr<Client::State> s) {
    std::unique_lock<std::mutex> lk(s->mu);
    for (;;) {
        // Wait for a drop (conn becomes null), terminal failure,
        // or shutdown.
        s->cv.wait(lk, [&] {
            return s->stop_requested || !s->conn;
        });
        if (s->stop_requested) return;
        if (s->terminal_failure) {
            // Already given up; exit and let the dtor join.
            return;
        }

        // Reconnect backoff loop.
        int attempt = 0;
        while (!s->conn && !s->stop_requested) {
            ++attempt;
            const auto host = s->host;
            const auto port = s->port;
            const auto timeout = s->opt.connect_timeout;
            lk.unlock();

            std::unique_ptr<transport::Connection> new_conn;
            try {
                auto fut = transport::tcp::connect(host, port);
                if (fut.wait_for(timeout)) {
                    new_conn = fut.get();
                }
            } catch (...) {
                // swallow; null new_conn means this attempt failed.
            }

            lk.lock();
            if (s->stop_requested) return;

            if (new_conn) {
                maybe_configure_keepalive(*new_conn, s->opt);
                s->conn = std::move(new_conn);
                s->consecutive_failures = 0;

                // Drain offline queue BEFORE notifying waiters,
                // so a just-unblocked send() doesn't interleave
                // with buffered ones. The drain releases the lock
                // briefly for each send to avoid holding it
                // across network I/O.
                while (!s->offline_queue.empty() && s->conn) {
                    auto msg = std::move(s->offline_queue.front());
                    s->offline_queue.pop_front();
                    auto* conn_raw = s->conn.get();
                    lk.unlock();
                    bool ok = true;
                    try { conn_raw->send_sync(msg); }
                    catch (...) { ok = false; }
                    lk.lock();
                    if (!ok) {
                        // Drop detected during drain. Reset and
                        // loop back to reconnect.
                        s->conn.reset();
                        break;
                    }
                }
                if (s->conn) {
                    s->cv.notify_all();
                    if (s->on_connected) {
                        auto cb = s->on_connected;
                        lk.unlock();
                        try { cb(); } catch (...) { /* swallow */ }
                        lk.lock();
                    }
                }
                break;    // out of backoff loop
            }

            // Failed attempt.
            ++s->consecutive_failures;
            const bool give_up =
                s->opt.reconnect_max_attempts > 0 &&
                s->consecutive_failures >= s->opt.reconnect_max_attempts;
            const auto delay = give_up
                ? std::chrono::milliseconds::zero()
                : compute_backoff(attempt + 1, s->opt);

            if (s->on_reconnect_failed) {
                auto cb = s->on_reconnect_failed;
                const int a = attempt;
                lk.unlock();
                try { cb(a, delay); } catch (...) { /* swallow */ }
                lk.lock();
            }
            if (s->stop_requested) return;
            if (give_up) {
                s->terminal_failure = true;
                s->cv.notify_all();
                return;
            }

            // Sleep the backoff. Interruptible via cv.wait_for
            // so stop_requested cuts it short.
            s->cv.wait_for(lk, delay, [&] {
                return s->stop_requested;
            });
            if (s->stop_requested) return;
        }
    }
}

}  // namespace

// ---------------------------------------------------------------
// Move / dtor
// ---------------------------------------------------------------

Client::Client(Client&& other) noexcept
    : state_(std::move(other.state_)),
      opt_(std::move(other.opt_)),
      dispatch_(std::move(other.dispatch_)),
      on_unknown_(std::move(other.on_unknown_)),
      on_error_(std::move(other.on_error_)),
      stop_requested_(other.stop_requested_.load()) {}

Client& Client::operator=(Client&& other) noexcept {
    if (this != &other) {
        state_ = std::move(other.state_);
        opt_ = std::move(other.opt_);
        dispatch_ = std::move(other.dispatch_);
        on_unknown_ = std::move(other.on_unknown_);
        on_error_ = std::move(other.on_error_);
        stop_requested_.store(other.stop_requested_.load());
    }
    return *this;
}

Client::~Client() {
    if (!state_) return;
    {
        std::lock_guard<std::mutex> lk(state_->mu);
        state_->stop_requested = true;
        if (state_->conn) {
            // Best-effort close so any in-flight receive unblocks.
            try { (void)state_->conn->close(); } catch (...) {}
        }
        state_->cv.notify_all();
    }
    if (state_->worker.joinable()) {
        state_->worker.join();
    }
}

// ---------------------------------------------------------------
// Factories
// ---------------------------------------------------------------

Client Client::connect(std::string host, std::uint16_t port,
                       ClientOptions opt) {
    std::exception_ptr last_err;
    std::unique_ptr<transport::Connection> conn;

    for (int attempt = 0; attempt <= opt.connect_retries; ++attempt) {
        auto fut = transport::tcp::connect(host, port);
        if (fut.wait_for(opt.connect_timeout)) {
            try {
                conn = fut.get();
                last_err = nullptr;
                break;
            } catch (...) {
                last_err = std::current_exception();
            }
        } else {
            fut.cancel();
            last_err = std::make_exception_ptr(transport::TimeoutError{});
        }
        if (attempt < opt.connect_retries) {
            std::this_thread::sleep_for(opt.retry_backoff);
        }
    }
    if (!conn) std::rethrow_exception(last_err);

    if (opt.tcp_keepalive) {
        maybe_configure_keepalive(*conn, opt);
    }

    Client c;
    c.state_ = std::make_shared<State>();
    c.state_->conn = std::move(conn);
    c.state_->host = std::move(host);
    c.state_->port = port;
    c.state_->opt = opt;
    c.opt_ = std::move(opt);

    if (c.opt_.auto_reconnect) {
        c.state_->worker = std::thread(worker_loop, c.state_);
    }
    return c;
}

Client Client::adopt(std::unique_ptr<transport::Connection> conn,
                     ClientOptions opt) {
    // adopt() has no way to re-dial — we don't know where `conn`
    // came from. auto_reconnect forced off silently.
    opt.auto_reconnect = false;
    Client c;
    c.state_ = std::make_shared<State>();
    c.state_->conn = std::move(conn);
    c.state_->opt = opt;
    c.opt_ = std::move(opt);
    return c;
}

// ---------------------------------------------------------------
// send_bytes — the non-template bottleneck
// ---------------------------------------------------------------

namespace {

// Precondition: lk is locked on state->mu, state->conn is non-null.
// Sends `wire` on the live connection. Releases the lock during
// the actual ::send to avoid holding it across kernel I/O.
// Re-acquires on return. On failure, nulls state->conn and
// notifies the worker.
bool send_live_unlocked(std::unique_lock<std::mutex>& lk,
                        Client::State& s,
                        const std::vector<std::uint8_t>& wire) {
    auto* c = s.conn.get();
    lk.unlock();
    bool ok = true;
    std::exception_ptr err;
    try {
        c->send_sync(wire);
    } catch (...) {
        ok = false;
        err = std::current_exception();
    }
    lk.lock();
    if (!ok) {
        s.conn.reset();
        s.cv.notify_all();
        if (s.on_disconnected) {
            auto cb = s.on_disconnected;
            lk.unlock();
            try { cb(err); } catch (...) {}
            lk.lock();
        }
    }
    return ok;
}

}  // namespace

void Client::send_bytes(std::vector<std::uint8_t>&& wire) {
    std::unique_lock<std::mutex> lk(state_->mu);

    if (state_->terminal_failure) {
        throw transport::ConnectionClosedError(
            "reconnect exhausted");
    }

    // Drain any queued messages first — matters when a user send()
    // lands immediately after a reconnect but before the worker
    // has had a chance to empty the queue.
    while (!state_->offline_queue.empty() && state_->conn) {
        auto msg = std::move(state_->offline_queue.front());
        state_->offline_queue.pop_front();
        if (!send_live_unlocked(lk, *state_, msg)) {
            // Drop detected mid-drain; fall through to the
            // offline-handling code below.
            break;
        }
    }

    if (state_->conn) {
        if (!send_live_unlocked(lk, *state_, wire)) {
            // Drop detected right now. If auto_reconnect is off,
            // surface the error to the caller. Otherwise fall
            // through and treat this call as a pre-reconnect
            // send — it gets buffered if possible.
            if (!opt_.auto_reconnect) {
                throw transport::ConnectionClosedError(
                    "send failed; peer closed");
            }
        } else {
            return;
        }
    }

    // Here: disconnected. Decide based on policy.
    if (!opt_.auto_reconnect) {
        throw transport::ConnectionClosedError("not connected");
    }

    if (opt_.offline_buffer_capacity == 0) {
        throw transport::ConnectionClosedError(
            "disconnected; offline buffer disabled");
    }

    using Policy = ClientOptions::OfflineOverflow;
    const auto cap = opt_.offline_buffer_capacity;

    if (state_->offline_queue.size() < cap) {
        state_->offline_queue.push_back(std::move(wire));
        return;
    }

    switch (opt_.offline_overflow_policy) {
    case Policy::DropOldest:
        state_->offline_queue.pop_front();
        state_->offline_queue.push_back(std::move(wire));
        return;
    case Policy::DropNewest:
        throw transport::BufferOverflowError{};
    case Policy::Block: {
        const auto deadline = std::chrono::steady_clock::now()
                            + opt_.send_timeout;
        if (!state_->cv.wait_until(lk, deadline, [&] {
                return state_->stop_requested
                    || state_->terminal_failure
                    || state_->offline_queue.size() < cap;
            })) {
            throw transport::TimeoutError{};
        }
        if (state_->stop_requested || state_->terminal_failure) {
            throw transport::ConnectionClosedError("aborted");
        }
        state_->offline_queue.push_back(std::move(wire));
        return;
    }
    }
}

// ---------------------------------------------------------------
// receive_one — the non-template bottleneck
// ---------------------------------------------------------------

transport::Incoming Client::receive_one(
        std::chrono::milliseconds timeout) {
    const auto deadline =
        std::chrono::steady_clock::now() + timeout;

    for (;;) {
        std::unique_lock<std::mutex> lk(state_->mu);
        if (state_->terminal_failure) {
            throw transport::ConnectionClosedError(
                "reconnect exhausted");
        }

        if (!state_->conn) {
            if (!opt_.auto_reconnect) {
                throw transport::ConnectionClosedError(
                    "not connected");
            }
            // Wait for the worker to restore a connection OR
            // give up OR the user to stop.
            if (!state_->cv.wait_until(lk, deadline, [&] {
                    return state_->stop_requested
                        || state_->terminal_failure
                        || state_->conn != nullptr;
                })) {
                throw transport::TimeoutError{};
            }
            if (state_->stop_requested) {
                throw transport::OperationCancelledError{};
            }
            if (state_->terminal_failure) {
                throw transport::ConnectionClosedError(
                    "reconnect exhausted");
            }
            // Loop to retry with the freshly connected conn.
            continue;
        }

        // Live connection. Compute remaining budget; a call that
        // has outlived its deadline short-circuits rather than
        // dropping into an indefinite read.
        const auto now = std::chrono::steady_clock::now();
        if (deadline <= now) {
            throw transport::TimeoutError{};
        }
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - now);

        auto* c = state_->conn.get();
        lk.unlock();

        try {
            return c->receive_sync(remaining);
        } catch (const transport::ConnectionClosedError&) {
            lk.lock();
            // Null the conn so the worker takes over.
            state_->conn.reset();
            state_->cv.notify_all();
            auto cb_ptr = std::current_exception();
            if (state_->on_disconnected) {
                auto cb = state_->on_disconnected;
                lk.unlock();
                try { cb(cb_ptr); } catch (...) {}
                lk.lock();
            }
            // If auto_reconnect is off, propagate. Else loop and
            // wait for a new conn.
            if (!opt_.auto_reconnect) throw;
            // Fall through to outer loop; lk goes out of scope.
        }
    }
}

transport::Incoming Client::receive_any() {
    constexpr auto kForever = std::chrono::milliseconds::max();
    if (opt_.receive_timeout >= kForever / 2) {
        return receive_one(std::chrono::hours(24 * 365));
    }
    return receive_one(opt_.receive_timeout);
}

// ---------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------

Client& Client::on_unknown(
        std::function<void(const transport::Incoming&)> handler) {
    on_unknown_ = std::move(handler);
    return *this;
}

Client& Client::on_error(
        std::function<void(std::exception_ptr)> handler) {
    on_error_ = std::move(handler);
    return *this;
}

Client& Client::on_connected(std::function<void()> h) {
    std::lock_guard<std::mutex> lk(state_->mu);
    state_->on_connected = std::move(h);
    return *this;
}

Client& Client::on_disconnected(
        std::function<void(std::exception_ptr)> h) {
    std::lock_guard<std::mutex> lk(state_->mu);
    state_->on_disconnected = std::move(h);
    return *this;
}

Client& Client::on_reconnect_failed(
        std::function<void(int, std::chrono::milliseconds)> h) {
    std::lock_guard<std::mutex> lk(state_->mu);
    state_->on_reconnect_failed = std::move(h);
    return *this;
}

// ---------------------------------------------------------------
// run / stop / close / connection
// ---------------------------------------------------------------

void Client::run() {
    stop_requested_ = false;
    while (!stop_requested_.load()) {
        transport::Incoming inc;
        try {
            inc = receive_one(std::chrono::milliseconds(250));
        } catch (const transport::TimeoutError&) {
            continue;
        } catch (const transport::ConnectionClosedError&) {
            if (on_error_) on_error_(std::current_exception());
            return;
        } catch (const transport::OperationCancelledError&) {
            return;
        } catch (...) {
            if (on_error_) {
                on_error_(std::current_exception());
                continue;
            }
            throw;
        }

        auto it = dispatch_.find(inc.header.type_id);
        try {
            if (it != dispatch_.end()) it->second(inc);
            else if (on_unknown_)       on_unknown_(inc);
        } catch (...) {
            if (on_error_) on_error_(std::current_exception());
            else throw;
        }
    }
}

void Client::stop() {
    stop_requested_ = true;
    if (!state_) return;
    std::lock_guard<std::mutex> lk(state_->mu);
    state_->stop_requested = true;
    if (state_->conn) {
        try { (void)state_->conn->close(); } catch (...) {}
    }
    state_->cv.notify_all();
}

void Client::close() {
    stop();
}

transport::Connection& Client::connection() {
    std::lock_guard<std::mutex> lk(state_->mu);
    if (!state_->conn) {
        throw transport::ConnectionClosedError(
            "connection() called while disconnected "
            "(auto_reconnect may be in progress)");
    }
    return *state_->conn;
}

}  // namespace oigtl
