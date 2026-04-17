// Loopback Connection — two peers wired to each other via shared
// in-memory inboxes. Used by tests to exercise the Connection /
// Framer / Future contract without real I/O.
//
// Threading model: synchronous. send() on one side immediately
// appends to the peer's inbox and, still under the shared lock,
// drains any pending receive() promises on the peer by invoking
// the framer. Continuations registered via Future::then() run
// inline on the sending thread. That's fine for tests; Phase 2's
// TCP backend has a real executor.

#include "oigtl/transport/connection.hpp"

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <utility>

#include "oigtl/transport/errors.hpp"

namespace oigtl::transport {

namespace {

// One-direction channel: A → B or B → A. The Framer parses the
// *receiver's* inbox, so a channel owns a Framer.
struct Channel {
    std::unique_ptr<Framer> framer;
    std::vector<std::uint8_t> inbox;
    std::deque<Promise<Incoming>> pending;
    bool closed = false;  // writer closed; reader drains then ECONN
};

// Shared state for a loopback pair. Holds both channels plus the
// single mutex guarding them.
struct LoopbackShared {
    std::mutex mu;
    // a_to_b: bytes A writes, B reads from.
    // b_to_a: bytes B writes, A reads from.
    Channel a_to_b;
    Channel b_to_a;
    std::atomic<std::uint64_t> next_id{0};
};

// Drain as many parseable messages as possible from a channel's
// inbox into the channel's pending-receive queue. Caller must hold
// the shared mutex. Returns promises to fulfill (caller invokes
// set_value outside the lock — though in this loopback impl the
// lock is short-lived and we don't bother).
void drain_pending(Channel& ch, std::mutex& /*locked*/) {
    while (!ch.pending.empty()) {
        try {
            auto inc = ch.framer->try_parse(ch.inbox);
            if (!inc) break;
            auto p = std::move(ch.pending.front());
            ch.pending.pop_front();
            p.set_value(std::move(*inc));
        } catch (...) {
            // Framer / codec error: deliver to the waiting
            // receiver, if any, and stop draining. The inbox
            // state is unknown after a parse throw, so we
            // leave it and let subsequent receive()s also
            // surface the issue.
            if (!ch.pending.empty()) {
                auto p = std::move(ch.pending.front());
                ch.pending.pop_front();
                p.set_exception(std::current_exception());
            }
            break;
        }
    }
}

class LoopbackConnection final : public Connection {
 public:
    LoopbackConnection(std::shared_ptr<LoopbackShared> shared,
                       bool is_a,
                       std::uint64_t peer_id)
        : shared_(std::move(shared)),
          is_a_(is_a),
          peer_id_(peer_id) {}

    std::optional<std::string>
    capability(std::string_view key) const override {
        if (key == "loopback.peer_id") return std::to_string(peer_id_);
        if (key == "loopback.side") return is_a_ ? "a" : "b";
        if (key == "framer") return "v3";
        return std::nullopt;
    }

    std::string peer_address() const override { return "loopback"; }
    std::uint16_t peer_port() const override { return 0; }
    std::uint16_t negotiated_version() const override { return 0; }

    Future<Incoming> receive() override {
        Promise<Incoming> p;
        auto fut = p.get_future();
        std::lock_guard<std::mutex> lk(shared_->mu);
        Channel& rx = is_a_ ? shared_->b_to_a : shared_->a_to_b;
        // Try immediate parse first — cheap common case when bytes
        // have already arrived.
        try {
            auto inc = rx.framer->try_parse(rx.inbox);
            if (inc) {
                p.set_value(std::move(*inc));
                return fut;
            }
        } catch (...) {
            p.set_exception(std::current_exception());
            return fut;
        }
        if (rx.closed) {
            p.set_exception(std::make_exception_ptr(
                ConnectionClosedError{}));
            return fut;
        }
        rx.pending.push_back(std::move(p));
        return fut;
    }

    Future<void> send(const std::uint8_t* wire,
                      std::size_t length) override {
        Promise<void> p;
        auto fut = p.get_future();
        std::lock_guard<std::mutex> lk(shared_->mu);
        Channel& tx = is_a_ ? shared_->a_to_b : shared_->b_to_a;
        if (tx.closed) {
            p.set_exception(std::make_exception_ptr(
                ConnectionClosedError{}));
            return fut;
        }
        auto framed = tx.framer->frame(wire, length);
        tx.inbox.insert(tx.inbox.end(), framed.begin(), framed.end());
        drain_pending(tx, shared_->mu);
        p.set_value();
        return fut;
    }

    Future<void> close() override {
        Promise<void> p;
        auto fut = p.get_future();
        std::lock_guard<std::mutex> lk(shared_->mu);
        // Close both directions I write to, and notify the peer's
        // receivers waiting for more from us.
        Channel& tx = is_a_ ? shared_->a_to_b : shared_->b_to_a;
        Channel& rx = is_a_ ? shared_->b_to_a : shared_->a_to_b;
        tx.closed = true;
        rx.closed = true;
        // Wake any parked receivers on our side.
        while (!rx.pending.empty()) {
            auto pp = std::move(rx.pending.front());
            rx.pending.pop_front();
            pp.set_exception(std::make_exception_ptr(
                ConnectionClosedError{}));
        }
        while (!tx.pending.empty()) {
            auto pp = std::move(tx.pending.front());
            tx.pending.pop_front();
            pp.set_exception(std::make_exception_ptr(
                ConnectionClosedError{}));
        }
        p.set_value();
        return fut;
    }

 private:
    std::shared_ptr<LoopbackShared> shared_;
    bool is_a_;
    std::uint64_t peer_id_;
};

}  // namespace

std::pair<std::unique_ptr<Connection>, std::unique_ptr<Connection>>
make_loopback_pair() {
    auto shared = std::make_shared<LoopbackShared>();
    shared->a_to_b.framer = make_v3_framer();
    shared->b_to_a.framer = make_v3_framer();
    const std::uint64_t id = shared->next_id.fetch_add(1);
    auto a = std::make_unique<LoopbackConnection>(shared, true, id);
    auto b = std::make_unique<LoopbackConnection>(shared, false, id);
    return {std::move(a), std::move(b)};
}

}  // namespace oigtl::transport
