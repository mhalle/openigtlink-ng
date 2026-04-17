// Client — non-template implementation details.

#include "oigtl/client.hpp"

#include <thread>

#include "oigtl/transport/errors.hpp"
#include "oigtl/transport/future.hpp"
#include "oigtl/transport/tcp.hpp"

namespace oigtl {

// Custom move — std::atomic<bool> can't be defaulted-moved.
Client::Client(Client&& other) noexcept
    : conn_(std::move(other.conn_)),
      opt_(std::move(other.opt_)),
      dispatch_(std::move(other.dispatch_)),
      on_unknown_(std::move(other.on_unknown_)),
      on_error_(std::move(other.on_error_)),
      stop_requested_(other.stop_requested_.load()) {}

Client& Client::operator=(Client&& other) noexcept {
    if (this != &other) {
        conn_ = std::move(other.conn_);
        opt_ = std::move(other.opt_);
        dispatch_ = std::move(other.dispatch_);
        on_unknown_ = std::move(other.on_unknown_);
        on_error_ = std::move(other.on_error_);
        stop_requested_.store(other.stop_requested_.load());
    }
    return *this;
}

Client::~Client() = default;

Client Client::connect(std::string host, std::uint16_t port,
                       ClientOptions opt) {
    std::exception_ptr last_err;
    for (int attempt = 0; attempt <= opt.connect_retries; ++attempt) {
        auto fut = transport::tcp::connect(host, port);
        if (fut.wait_for(opt.connect_timeout)) {
            try {
                auto conn = fut.get();
                Client c;
                c.conn_ = std::move(conn);
                c.opt_ = std::move(opt);
                return c;
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
    std::rethrow_exception(last_err);
}

Client Client::adopt(std::unique_ptr<transport::Connection> conn,
                     ClientOptions opt) {
    Client c;
    c.conn_ = std::move(conn);
    c.opt_ = std::move(opt);
    return c;
}

transport::Incoming Client::receive_any() {
    auto fut = conn_->receive();
    if (!fut.wait_for(opt_.receive_timeout)) {
        fut.cancel();
        throw transport::TimeoutError{};
    }
    return fut.get();
}

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

void Client::run() {
    stop_requested_ = false;
    while (!stop_requested_.load()) {
        transport::Incoming inc;
        try {
            // Use shorter polling so stop_requested() responds
            // promptly even if no message arrives.
            auto fut = conn_->receive();
            const auto poll = std::chrono::milliseconds(250);
            while (!fut.wait_for(poll)) {
                if (stop_requested_.load()) {
                    fut.cancel();
                    return;
                }
            }
            inc = fut.get();
        } catch (const transport::ConnectionClosedError&) {
            if (on_error_) {
                on_error_(std::current_exception());
            }
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
            if (it != dispatch_.end()) {
                it->second(inc);
            } else if (on_unknown_) {
                on_unknown_(inc);
            }
        } catch (...) {
            if (on_error_) {
                on_error_(std::current_exception());
            } else {
                throw;
            }
        }
    }
}

void Client::stop() {
    stop_requested_ = true;
    if (conn_) {
        // Best effort: close the connection so any in-flight
        // receive unblocks. The run() loop then sees the
        // ConnectionClosedError or OperationCancelledError and
        // exits.
        (void)conn_->close();
    }
}

void Client::close() {
    if (!conn_) return;
    auto fut = conn_->close();
    (void)fut.wait_for(std::chrono::milliseconds(250));
}

}  // namespace oigtl
