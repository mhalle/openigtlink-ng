// TCP Connection + Acceptor — real-socket backend driven by the
// single library-internal asio::io_context.
//
// Threading discipline:
//   - All Impl state (socket, inbox, pending queues, closed flag)
//     is mutated only on the io_context thread.
//   - Public Connection/Acceptor methods post a lambda onto that
//     thread and return a Future<T> — the Promise is the cross-
//     thread bridge.
//   - Framer.try_parse() is called on the io_context thread only.
//
// Lifetime discipline:
//   - Impl inherits enable_shared_from_this so async handlers
//     capture a shared_ptr and keep the object alive for the
//     duration of pending ops.
//   - Public wrapper (TcpConnection / TcpAcceptor) holds one
//     shared_ptr<Impl>. Destruction requests graceful close; any
//     handlers still in flight own their own shared_ptr and
//     complete cleanly.

#include <asio.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <sys/socket.h>    // ::send, MSG_NOSIGNAL
#include <poll.h>          // ::poll for EAGAIN recovery
#include <cerrno>

#include "io_runtime.hpp"
#include "oigtl/transport/connection.hpp"
#include "oigtl/transport/errors.hpp"
#include "oigtl/transport/framer.hpp"
#include "oigtl/transport/future.hpp"
#include "oigtl/transport/tcp.hpp"

namespace oigtl::transport::tcp {

namespace {

using asio::ip::tcp;

// ===========================================================================
// TcpConnection::Impl — lives on the io_context thread.
// ===========================================================================
struct ConnImpl : std::enable_shared_from_this<ConnImpl> {
    tcp::socket socket;
    std::unique_ptr<Framer> framer;
    std::vector<std::uint8_t> inbox;
    std::array<std::uint8_t, 8192> rx_scratch{};
    std::deque<Promise<Incoming>> pending_receives;
    std::string peer_addr;
    std::uint16_t peer_port_ = 0;
    bool closed = false;
    bool read_armed = false;

    // Serialises concurrent send_sync callers against each other
    // and against the framer used to wrap outbound bytes.
    std::mutex send_sync_mu;

    explicit ConnImpl(asio::io_context& ctx)
        : socket(ctx), framer(make_v3_framer()) {}

    // --- must be called on io_ctx thread ---

    void arm_read_locked() {
        if (read_armed || closed) return;
        read_armed = true;
        auto self = shared_from_this();
        socket.async_read_some(
            asio::buffer(rx_scratch),
            [self](const asio::error_code& ec, std::size_t n) {
                self->read_armed = false;
                if (ec) {
                    self->handle_read_error(ec);
                    return;
                }
                self->inbox.insert(self->inbox.end(),
                                   self->rx_scratch.begin(),
                                   self->rx_scratch.begin() + n);
                self->drain_inbox();
                if (!self->pending_receives.empty() && !self->closed) {
                    self->arm_read_locked();
                }
            });
    }

    void handle_read_error(const asio::error_code& ec) {
        closed = true;
        auto err = (ec == asio::error::eof ||
                    ec == asio::error::connection_reset ||
                    ec == asio::error::operation_aborted)
            ? std::make_exception_ptr(ConnectionClosedError{})
            : std::make_exception_ptr(ConnectionClosedError(ec.message()));
        while (!pending_receives.empty()) {
            auto p = std::move(pending_receives.front());
            pending_receives.pop_front();
            p.set_exception(err);
        }
        asio::error_code ignored;
        socket.close(ignored);
    }

    // Parse as many messages as the inbox holds; fulfill waiting
    // promises in order. A framer/codec throw is delivered to the
    // front-of-queue receive and stops further draining.
    void drain_inbox() {
        while (!pending_receives.empty()) {
            try {
                auto inc = framer->try_parse(inbox);
                if (!inc) return;
                auto p = std::move(pending_receives.front());
                pending_receives.pop_front();
                p.set_value(std::move(*inc));
            } catch (...) {
                auto p = std::move(pending_receives.front());
                pending_receives.pop_front();
                p.set_exception(std::current_exception());
                // Parse state after a throw is suspect; close out.
                closed = true;
                asio::error_code ignored;
                socket.close(ignored);
                while (!pending_receives.empty()) {
                    auto q = std::move(pending_receives.front());
                    pending_receives.pop_front();
                    q.set_exception(std::make_exception_ptr(
                        ConnectionClosedError{}));
                }
                return;
            }
        }
    }

    void post_close(Promise<void> p) {
        auto self = shared_from_this();
        asio::post(socket.get_executor(),
            [self, p = std::move(p)]() mutable {
                if (!self->closed) {
                    self->closed = true;
                    asio::error_code ignored;
                    self->socket.shutdown(
                        tcp::socket::shutdown_both, ignored);
                    self->socket.close(ignored);
                    while (!self->pending_receives.empty()) {
                        auto q = std::move(
                            self->pending_receives.front());
                        self->pending_receives.pop_front();
                        q.set_exception(std::make_exception_ptr(
                            ConnectionClosedError{}));
                    }
                }
                p.set_value();
            });
    }
};

// ===========================================================================
// TcpConnection — public Connection facade over ConnImpl
// ===========================================================================
class TcpConnection final : public Connection {
 public:
    explicit TcpConnection(std::shared_ptr<ConnImpl> impl)
        : impl_(std::move(impl)) {}

    ~TcpConnection() override {
        // Fire-and-forget close. If the user already closed, this
        // is a no-op; otherwise it schedules a shutdown and we let
        // it complete in the background.
        Promise<void> p;
        (void)p.get_future();  // discard future
        impl_->post_close(std::move(p));
    }

    std::optional<std::string>
    capability(std::string_view key) const override {
        if (key == "tcp.peer_address") return impl_->peer_addr;
        if (key == "tcp.peer_port")
            return std::to_string(impl_->peer_port_);
        if (key == "framer") return "v3";
        return std::nullopt;
    }

    std::string peer_address() const override { return impl_->peer_addr; }
    std::uint16_t peer_port() const override { return impl_->peer_port_; }
    std::uint16_t negotiated_version() const override { return 0; }

    Future<Incoming> receive() override {
        Promise<Incoming> p;
        auto f = p.get_future();
        auto impl = impl_;
        asio::post(impl->socket.get_executor(),
            [impl, p = std::move(p)]() mutable {
                if (impl->closed && impl->inbox.empty()) {
                    p.set_exception(std::make_exception_ptr(
                        ConnectionClosedError{}));
                    return;
                }
                impl->pending_receives.push_back(std::move(p));
                // Try parsing what's already in the inbox first.
                impl->drain_inbox();
                if (!impl->pending_receives.empty() && !impl->closed) {
                    impl->arm_read_locked();
                }
            });
        return f;
    }

    // Direct POSIX write in the caller's thread — bypasses the
    // io_context thread-hop. Safe concurrently with the io-thread-
    // owned async_read (kernel handles TCP full-duplex); a mutex
    // serialises concurrent send_sync callers so bytes don't
    // interleave.
    void send_sync(const std::uint8_t* wire,
                   std::size_t length) override {
        std::lock_guard<std::mutex> lk(impl_->send_sync_mu);
        if (impl_->closed) {
            throw ConnectionClosedError{};
        }
        // v3 framer::frame is identity; skip the vector copy on
        // the fast path. A future non-identity framer (v4 chunked)
        // will need to materialise and we'll fall back through
        // framer->frame().
        const std::uint8_t* buf = wire;
        std::size_t buf_len = length;
        std::vector<std::uint8_t> framed_store;
        if (impl_->framer->name() != "v3") {
            framed_store = impl_->framer->frame(wire, length);
            buf = framed_store.data();
            buf_len = framed_store.size();
        }

        const int fd = impl_->socket.native_handle();
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;  // Linux: don't SIGPIPE
#else
        const int flags = 0;             // macOS: set SO_NOSIGPIPE elsewhere
#endif
        std::size_t off = 0;
        while (off < buf_len) {
            ssize_t n = ::send(fd, buf + off,
                               buf_len - off, flags);
            if (n > 0) { off += static_cast<std::size_t>(n); continue; }
            if (n < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // asio keeps the fd non-blocking; wait for the
                    // kernel send buffer to drain.
                    struct pollfd pfd{};
                    pfd.fd = fd;
                    pfd.events = POLLOUT;
                    int r = ::poll(&pfd, 1, -1);
                    if (r < 0 && errno != EINTR) {
                        throw ConnectionClosedError(
                            std::string("poll: ") +
                            std::strerror(errno));
                    }
                    continue;
                }
                throw ConnectionClosedError(
                    std::string("send: ") + std::strerror(errno));
            }
        }
    }

    Future<void> send(const std::uint8_t* wire,
                      std::size_t length) override {
        Promise<void> p;
        auto f = p.get_future();
        auto impl = impl_;
        // Copy bytes into an owning buffer — the user's wire[] may
        // disappear before async_write completes.
        auto bytes = std::make_shared<std::vector<std::uint8_t>>(
            impl->framer->frame(wire, length));
        asio::post(impl->socket.get_executor(),
            [impl, bytes, p = std::move(p)]() mutable {
                if (impl->closed) {
                    p.set_exception(std::make_exception_ptr(
                        ConnectionClosedError{}));
                    return;
                }
                asio::async_write(impl->socket, asio::buffer(*bytes),
                    [impl, bytes, p = std::move(p)](
                        const asio::error_code& ec,
                        std::size_t /*n*/) mutable {
                        if (ec) {
                            p.set_exception(std::make_exception_ptr(
                                ConnectionClosedError(ec.message())));
                            return;
                        }
                        p.set_value();
                    });
            });
        return f;
    }

    Future<void> close() override {
        Promise<void> p;
        auto f = p.get_future();
        impl_->post_close(std::move(p));
        return f;
    }

 private:
    std::shared_ptr<ConnImpl> impl_;
};

// Helper: populate peer_addr / peer_port from a connected socket,
// and set TCP_NODELAY — small-frame latency matters for real-time
// tracking streams and for loopback throughput (Nagle defers sub-
// MSS packets waiting for ACKs otherwise).
void populate_peer(ConnImpl& impl) {
    asio::error_code ec;
    impl.socket.set_option(tcp::no_delay(true), ec);
    ec.clear();

    // macOS has no MSG_NOSIGNAL; set SO_NOSIGPIPE instead so a
    // send() on a closed peer doesn't kill the process.
#ifdef SO_NOSIGPIPE
    int on = 1;
    (void)::setsockopt(impl.socket.native_handle(), SOL_SOCKET,
                       SO_NOSIGPIPE, &on, sizeof(on));
#endif

    auto ep = impl.socket.remote_endpoint(ec);
    if (!ec) {
        impl.peer_addr = ep.address().to_string();
        impl.peer_port_ = ep.port();
    }
}

// ===========================================================================
// TcpAcceptor::Impl
// ===========================================================================
struct AccImpl : std::enable_shared_from_this<AccImpl> {
    tcp::acceptor acceptor;
    std::deque<Promise<std::unique_ptr<Connection>>> pending;
    bool closed = false;
    bool accept_armed = false;

    explicit AccImpl(asio::io_context& ctx, const tcp::endpoint& ep)
        : acceptor(ctx, ep) {}

    void arm_accept_locked() {
        if (accept_armed || closed || pending.empty()) return;
        accept_armed = true;
        auto self = shared_from_this();
        auto incoming_impl = std::make_shared<ConnImpl>(detail::io_ctx());
        acceptor.async_accept(
            incoming_impl->socket,
            [self, incoming_impl](const asio::error_code& ec) {
                self->accept_armed = false;
                if (ec || self->closed) {
                    if (!self->pending.empty()) {
                        auto p = std::move(self->pending.front());
                        self->pending.pop_front();
                        p.set_exception(std::make_exception_ptr(
                            ec == asio::error::operation_aborted
                                ? OperationCancelledError{}
                                : OperationCancelledError{}));
                    }
                    return;
                }
                populate_peer(*incoming_impl);
                if (!self->pending.empty()) {
                    auto p = std::move(self->pending.front());
                    self->pending.pop_front();
                    p.set_value(std::make_unique<TcpConnection>(
                        incoming_impl));
                }
                if (!self->pending.empty() && !self->closed) {
                    self->arm_accept_locked();
                }
            });
    }
};

class TcpAcceptor final : public Acceptor {
 public:
    explicit TcpAcceptor(std::shared_ptr<AccImpl> impl)
        : impl_(std::move(impl)) {}

    ~TcpAcceptor() override {
        // Best-effort close.
        Promise<void> p;
        (void)p.get_future();
        auto impl = impl_;
        asio::post(impl->acceptor.get_executor(),
            [impl, p = std::move(p)]() mutable {
                if (!impl->closed) {
                    impl->closed = true;
                    asio::error_code ec;
                    impl->acceptor.close(ec);
                    while (!impl->pending.empty()) {
                        auto q = std::move(impl->pending.front());
                        impl->pending.pop_front();
                        q.set_exception(std::make_exception_ptr(
                            OperationCancelledError{}));
                    }
                }
                p.set_value();
            });
    }

    Future<std::unique_ptr<Connection>> accept() override {
        Promise<std::unique_ptr<Connection>> p;
        auto f = p.get_future();
        auto impl = impl_;
        asio::post(impl->acceptor.get_executor(),
            [impl, p = std::move(p)]() mutable {
                if (impl->closed) {
                    p.set_exception(std::make_exception_ptr(
                        ConnectionClosedError{"acceptor closed"}));
                    return;
                }
                impl->pending.push_back(std::move(p));
                impl->arm_accept_locked();
            });
        return f;
    }

    std::uint16_t local_port() const override {
        asio::error_code ec;
        auto ep = impl_->acceptor.local_endpoint(ec);
        return ec ? 0 : ep.port();
    }

    Future<void> close() override {
        Promise<void> p;
        auto f = p.get_future();
        auto impl = impl_;
        asio::post(impl->acceptor.get_executor(),
            [impl, p = std::move(p)]() mutable {
                if (!impl->closed) {
                    impl->closed = true;
                    asio::error_code ec;
                    impl->acceptor.close(ec);
                    while (!impl->pending.empty()) {
                        auto q = std::move(impl->pending.front());
                        impl->pending.pop_front();
                        q.set_exception(std::make_exception_ptr(
                            OperationCancelledError{}));
                    }
                }
                p.set_value();
            });
        return f;
    }

 private:
    std::shared_ptr<AccImpl> impl_;
};

}  // namespace

// ===========================================================================
// Public factories
// ===========================================================================

Future<std::unique_ptr<Connection>>
connect(std::string host, std::uint16_t port) {
    Promise<std::unique_ptr<Connection>> p;
    auto f = p.get_future();

    auto& ctx = detail::io_ctx();
    auto impl = std::make_shared<ConnImpl>(ctx);
    auto resolver = std::make_shared<tcp::resolver>(ctx);

    asio::post(ctx,
        [resolver, impl, host, port, p = std::move(p)]() mutable {
            asio::error_code ec;
            auto results = resolver->resolve(
                host, std::to_string(port), ec);
            if (ec) {
                p.set_exception(std::make_exception_ptr(
                    ConnectionClosedError("resolve: " + ec.message())));
                return;
            }
            asio::async_connect(
                impl->socket, results,
                [impl, p = std::move(p)](
                    const asio::error_code& cec,
                    const tcp::endpoint&) mutable {
                    if (cec) {
                        p.set_exception(std::make_exception_ptr(
                            ConnectionClosedError(
                                "connect: " + cec.message())));
                        return;
                    }
                    populate_peer(*impl);
                    p.set_value(std::make_unique<TcpConnection>(impl));
                });
        });

    return f;
}

std::unique_ptr<Acceptor>
listen(std::uint16_t port, std::string bind_address) {
    auto& ctx = detail::io_ctx();
    asio::error_code ec;
    auto addr = asio::ip::make_address(bind_address, ec);
    if (ec) {
        throw ConnectionClosedError(
            "invalid bind address: " + bind_address);
    }
    tcp::endpoint ep(addr, port);
    auto impl = std::make_shared<AccImpl>(ctx, ep);
    impl->acceptor.set_option(tcp::acceptor::reuse_address(true));
    return std::make_unique<TcpAcceptor>(impl);
}

}  // namespace oigtl::transport::tcp
