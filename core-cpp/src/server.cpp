// Server implementation — accept loop + per-peer dispatch threads.

#include "oigtl/server.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>

#include "oigtl/transport/detail/net_compat.hpp"
#include "oigtl/transport/errors.hpp"
#include "oigtl/transport/future.hpp"
#include "oigtl/transport/policy.hpp"
#include "oigtl/transport/tcp.hpp"

namespace oigtl {

// Custom move — std::atomic<bool> + std::mutex can't be defaulted.
Server::Server(Server&& other) noexcept
    : acceptor_(std::move(other.acceptor_)),
      opt_(std::move(other.opt_)),
      installers_(std::move(other.installers_)),
      on_unknown_(std::move(other.on_unknown_)),
      on_connected_(std::move(other.on_connected_)),
      on_disconnected_(std::move(other.on_disconnected_)),
      on_error_(std::move(other.on_error_)),
      stop_requested_(other.stop_requested_.load()),
      peer_threads_(std::move(other.peer_threads_)) {}

Server& Server::operator=(Server&& other) noexcept {
    if (this != &other) {
        acceptor_ = std::move(other.acceptor_);
        opt_ = std::move(other.opt_);
        installers_ = std::move(other.installers_);
        on_unknown_ = std::move(other.on_unknown_);
        on_connected_ = std::move(other.on_connected_);
        on_disconnected_ = std::move(other.on_disconnected_);
        on_error_ = std::move(other.on_error_);
        stop_requested_.store(other.stop_requested_.load());
        peer_threads_ = std::move(other.peer_threads_);
    }
    return *this;
}

Server::~Server() {
    // Shut down any still-running peer threads.
    stop();
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lk(peers_mu_);
        threads.swap(peer_threads_);
    }
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}

Server Server::listen(std::uint16_t port, ServerOptions opt) {
    Server s;
    // Hand the policy to the acceptor at listen time. Any further
    // builder calls (restrict_to_this_machine_only(), etc.) will
    // push updates to the live acceptor via set_policy().
    s.acceptor_ = transport::tcp::listen(port, opt.bind_address,
                                         opt.policy);
    s.opt_ = std::move(opt);
    return s;
}

Client Server::accept() {
    auto fut = acceptor_->accept();
    auto conn = fut.get();
    return Client::adopt(std::move(conn), opt_.client_defaults);
}

Server& Server::on_unknown(
        std::function<void(Client&, const transport::Incoming&)> h) {
    on_unknown_ = std::move(h);
    return *this;
}

Server& Server::on_connected(std::function<void(Client&)> h) {
    on_connected_ = std::move(h);
    return *this;
}

Server& Server::on_disconnected(std::function<void(Client&)> h) {
    on_disconnected_ = std::move(h);
    return *this;
}

Server& Server::on_error(
        std::function<void(Client&, std::exception_ptr)> h) {
    on_error_ = std::move(h);
    return *this;
}

void Server::serve_peer(std::unique_ptr<transport::Connection> conn) {
    Client c = Client::adopt(std::move(conn), opt_.client_defaults);

    // Install handlers.
    for (auto& installer : installers_) installer(c);

    if (on_unknown_) {
        auto h = on_unknown_;
        c.on_unknown([&c, h](const transport::Incoming& inc) {
            h(c, inc);
        });
    }
    if (on_error_) {
        auto h = on_error_;
        c.on_error([&c, h](std::exception_ptr ep) { h(c, ep); });
    }

    try {
        if (on_connected_) on_connected_(c);
        c.run();
    } catch (...) {
        if (on_error_) on_error_(c, std::current_exception());
    }

    if (on_disconnected_) {
        try { on_disconnected_(c); }
        catch (...) { /* swallow */ }
    }
}

void Server::run() {
    stop_requested_ = false;
    while (!stop_requested_.load()) {
        std::unique_ptr<transport::Connection> conn;
        try {
            auto fut = acceptor_->accept();
            // Poll so stop_requested gets noticed even if idle.
            const auto poll = std::chrono::milliseconds(250);
            while (!fut.wait_for(poll)) {
                if (stop_requested_.load()) {
                    fut.cancel();
                    break;
                }
            }
            if (stop_requested_.load()) break;
            conn = fut.get();
        } catch (const transport::OperationCancelledError&) {
            break;
        } catch (const transport::ConnectionClosedError&) {
            break;
        } catch (...) {
            if (on_error_) {
                // No peer to attach yet; pass a dummy closed Client.
                try { std::rethrow_exception(
                          std::current_exception()); }
                catch (...) { /* accept errors are fatal */ }
                throw;
            }
            throw;
        }

        // Cap concurrent peers: reap finished threads.
        {
            std::lock_guard<std::mutex> lk(peers_mu_);
            peer_threads_.erase(
                std::remove_if(
                    peer_threads_.begin(), peer_threads_.end(),
                    [](std::thread& t) {
                        if (!t.joinable()) return true;
                        return false;
                    }),
                peer_threads_.end());
            while (peer_threads_.size() >= opt_.max_connections) {
                // Join the oldest.
                auto t = std::move(peer_threads_.front());
                peer_threads_.erase(peer_threads_.begin());
                // Release the lock so the thread can exit cleanly.
                peers_mu_.unlock();
                if (t.joinable()) t.join();
                peers_mu_.lock();
            }
        }

        std::lock_guard<std::mutex> lk(peers_mu_);
        peer_threads_.emplace_back(
            [this, c = std::move(conn)]() mutable {
                serve_peer(std::move(c));
            });
    }
}

void Server::stop() {
    stop_requested_ = true;
    if (acceptor_) (void)acceptor_->close();
}

std::uint16_t Server::local_port() const {
    return acceptor_ ? acceptor_->local_port() : 0;
}

void Server::close() { stop(); }

// ---------------------------------------------------------------
// Policy builders. Each mutates opt_.policy and, if the acceptor
// is already live, pushes the update. That means builder calls
// work both before `listen()` (via pre-populating options) and
// after, matching the compat shim's ergonomics.
// ---------------------------------------------------------------

namespace {

// Helper: push the current policy to the live acceptor if one
// exists. Pulled into a lambda inside each builder would be
// clearer, but we want this to stay uninlined so the acceptor
// pointer isn't re-read in every builder.
void push_to_live(transport::Acceptor* acc,
                  const transport::PeerPolicy& p) {
    if (acc) acc->set_policy(p);
}

// Fill out an allow-list for every active non-link-local
// interface on this host. If `only_iface` is non-empty, restrict
// to that one interface; loopback is always added separately
// unless the caller deliberately picked a non-loopback interface
// (matching the compat shim's behaviour).
void apply_local_subnet(transport::PeerPolicy& pol,
                        const std::string& only_iface) {
    auto ifaces = transport::enumerate_interfaces();
    bool had_loopback = false;
    for (const auto& ia : ifaces) {
        if (!only_iface.empty() && ia.name != only_iface) continue;
        if (ia.is_link_local) continue;
        pol.allowed_peers.push_back(ia.subnet);
        if (ia.is_loopback) had_loopback = true;
    }
    if (!had_loopback) {
        if (auto r = transport::parse("127.0.0.0/8")) {
            pol.allowed_peers.push_back(*r);
        }
    }
}

}  // namespace

Server& Server::restrict_to_this_machine_only() {
    if (auto r = transport::parse("127.0.0.0/8")) {
        opt_.policy.allowed_peers.push_back(*r);
    }
    if (auto r = transport::parse("::1")) {
        opt_.policy.allowed_peers.push_back(*r);
    }
    push_to_live(acceptor_.get(), opt_.policy);
    return *this;
}

Server& Server::restrict_to_local_subnet() {
    apply_local_subnet(opt_.policy, /*only_iface=*/"");
    push_to_live(acceptor_.get(), opt_.policy);
    return *this;
}

Server& Server::restrict_to_local_subnet(const std::string& iface_name) {
    apply_local_subnet(opt_.policy, iface_name);
    push_to_live(acceptor_.get(), opt_.policy);
    return *this;
}

Server& Server::allow_peer(const std::string& ip_or_host) {
    // Literal IP / CIDR / range first — same parse chain as
    // the compat shim.
    if (auto r = transport::parse(ip_or_host)) {
        opt_.policy.allowed_peers.push_back(*r);
        push_to_live(acceptor_.get(), opt_.policy);
        return *this;
    }
    // Hostname resolution fallthrough. Resolved entries become
    // single-host ranges.
    auto resolved = transport::detail::resolve_hostname(ip_or_host);
    for (const auto& r : resolved) {
        if (auto range = transport::parse_ip(
                transport::detail::format_ip(r.family, r.bytes))) {
            opt_.policy.allowed_peers.push_back(*range);
        }
    }
    push_to_live(acceptor_.get(), opt_.policy);
    return *this;
}

Server& Server::allow_peer_range(const std::string& first_ip,
                                 const std::string& last_ip) {
    if (auto r = transport::parse_range(first_ip, last_ip)) {
        opt_.policy.allowed_peers.push_back(*r);
        push_to_live(acceptor_.get(), opt_.policy);
    }
    return *this;
}

Server& Server::set_max_simultaneous_clients(std::size_t n) {
    opt_.policy.max_concurrent_connections = n;
    push_to_live(acceptor_.get(), opt_.policy);
    return *this;
}

Server& Server::disconnect_if_silent_for(std::chrono::seconds t) {
    opt_.policy.idle_timeout = t;
    push_to_live(acceptor_.get(), opt_.policy);
    return *this;
}

Server& Server::set_max_message_size_bytes(std::size_t n) {
    opt_.policy.max_message_size = n;
    push_to_live(acceptor_.get(), opt_.policy);
    return *this;
}

Server& Server::set_policy(transport::PeerPolicy p) {
    opt_.policy = std::move(p);
    push_to_live(acceptor_.get(), opt_.policy);
    return *this;
}

}  // namespace oigtl
