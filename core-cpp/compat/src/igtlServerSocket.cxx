// igtlServerSocket.cxx — binds via oigtl::transport::tcp::listen()
// and yields accepted Connections as ClientSocket instances.
//
// Timeout on WaitForConnection: the ergonomic thing is
// `Future<...>::wait_for(msec)`, which returns true on resolution
// and false on timeout. On timeout we cancel the pending accept
// so the next call gets a fresh one. (Transport's cancel() is
// cooperative — the asio backend honors it.)

#include "igtl/igtlServerSocket.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "oigtl/transport/connection.hpp"
#include "oigtl/transport/detail/net_compat.hpp"
#include "oigtl/transport/policy.hpp"
#include "oigtl/transport/tcp.hpp"

namespace igtl {

struct ServerSocket::AcceptorPimpl {
    std::unique_ptr<oigtl::transport::Acceptor> acceptor;
    int server_port = 0;

    // Pending policy: accumulated by Restrict*/Allow*/Set* before
    // or after CreateServer(). On CreateServer we pass it to
    // tcp::listen; on each subsequent mutator we update the live
    // acceptor in place.
    oigtl::transport::PeerPolicy policy;

    // Push `policy` to the live acceptor if it exists.
    void apply_live() {
        if (acceptor) acceptor->set_policy(policy);
    }
};

ServerSocket::ServerSocket()  : m_AcceptorPimpl(new AcceptorPimpl) {}
ServerSocket::~ServerSocket() {
    if (m_AcceptorPimpl && m_AcceptorPimpl->acceptor) {
        try {
            m_AcceptorPimpl->acceptor->close().get();
        } catch (...) {}
    }
}

int ServerSocket::CreateServer(int port) {
    try {
        auto acc = oigtl::transport::tcp::listen(
            static_cast<std::uint16_t>(port),
            // Bind to all interfaces by default — matches
            // upstream's ServerSocket, which uses INADDR_ANY.
            "0.0.0.0",
            m_AcceptorPimpl->policy);
        m_AcceptorPimpl->server_port = acc->local_port();
        m_AcceptorPimpl->acceptor    = std::move(acc);
        return 0;
    } catch (...) {
        return -1;
    }
}

ClientSocket::Pointer ServerSocket::WaitForConnection(unsigned long msec) {
    if (!m_AcceptorPimpl->acceptor) return ClientSocket::Pointer();

    auto fut = m_AcceptorPimpl->acceptor->accept();
    if (msec != 0) {
        if (!fut.wait_for(std::chrono::milliseconds(msec))) {
            fut.cancel();
            return ClientSocket::Pointer();
        }
    }
    std::unique_ptr<oigtl::transport::Connection> conn;
    try {
        conn = fut.get();
    } catch (...) {
        return ClientSocket::Pointer();
    }
    auto cs = ClientSocket::New();
    cs->AttachConnection(static_cast<void*>(&conn));
    return cs;
}

int ServerSocket::GetServerPort() {
    return m_AcceptorPimpl ? m_AcceptorPimpl->server_port : 0;
}

// ===== Access-restriction mutators ===============================

void ServerSocket::RestrictToThisMachineOnly() {
    namespace t = oigtl::transport;
    auto& pol = m_AcceptorPimpl->policy;
    // Both IPv4 and IPv6 loopback, single pair of calls.
    if (auto r = t::parse("127.0.0.0/8")) pol.allowed_peers.push_back(*r);
    if (auto r = t::parse("::1"))         pol.allowed_peers.push_back(*r);
    m_AcceptorPimpl->apply_live();
}

namespace {

// Helper: add each non-loopback, non-link-local interface's subnet
// to the allow-list; always add loopback separately so local
// tooling isn't locked out. Also print a summary line for the
// operator's confirmation.
void apply_local_subnet(oigtl::transport::PeerPolicy& pol,
                        const std::string& only_iface) {
    namespace t = oigtl::transport;
    auto ifaces = t::enumerate_interfaces();

    std::string summary;
    auto add = [&](const t::InterfaceAddress& ia, const char* tag) {
        pol.allowed_peers.push_back(ia.subnet);
        if (!summary.empty()) summary += ", ";
        summary += ia.subnet.to_string() + " (" + ia.name +
                   (tag[0] ? (std::string(" ") + tag) : "") + ")";
    };

    bool had_loopback = false;
    for (const auto& ia : ifaces) {
        if (!only_iface.empty() && ia.name != only_iface) continue;
        if (ia.is_link_local) continue;
        if (ia.is_loopback) { add(ia, "loopback"); had_loopback = true; continue; }
        add(ia, "");
    }
    if (!had_loopback) {
        // Always include loopback unless the caller explicitly
        // restricted to a non-loopback interface. A researcher
        // binding to `eth0` still expects a local viewer on the
        // same box to work.
        if (auto r = t::parse("127.0.0.0/8")) {
            pol.allowed_peers.push_back(*r);
            summary += (summary.empty() ? "" : ", ");
            summary += "127.0.0.0/8 (loopback)";
        }
    }
    std::fprintf(stderr,
                 "[igtl] RestrictToLocalSubnet: allowing %s\n",
                 summary.empty() ? "(none)" : summary.c_str());
}

}  // namespace

void ServerSocket::RestrictToLocalSubnet() {
    apply_local_subnet(m_AcceptorPimpl->policy, /*only_iface=*/"");
    m_AcceptorPimpl->apply_live();
}

void ServerSocket::RestrictToLocalSubnet(const std::string& iface) {
    apply_local_subnet(m_AcceptorPimpl->policy, iface);
    m_AcceptorPimpl->apply_live();
}

int ServerSocket::AllowPeer(const std::string& ip_or_host) {
    namespace t = oigtl::transport;
    auto& pol = m_AcceptorPimpl->policy;

    // Try literal IP/CIDR/range first.
    if (auto r = t::parse(ip_or_host)) {
        pol.allowed_peers.push_back(*r);
        m_AcceptorPimpl->apply_live();
        return 1;
    }

    // Hostname: resolve via the platform abstraction and add each
    // A / AAAA as a single-host range.
    namespace d = oigtl::transport::detail;
    auto resolved = d::resolve_hostname(ip_or_host);
    int added = 0;
    for (const auto& r : resolved) {
        if (auto range = t::parse_ip(d::format_ip(r.family, r.bytes))) {
            pol.allowed_peers.push_back(*range);
            ++added;
        }
    }
    if (added > 0) {
        m_AcceptorPimpl->apply_live();
        return 1;
    }
    return 0;
}

int ServerSocket::AllowPeerRange(const std::string& first_ip,
                                 const std::string& last_ip) {
    namespace t = oigtl::transport;
    auto r = t::parse_range(first_ip, last_ip);
    if (!r) return 0;
    m_AcceptorPimpl->policy.allowed_peers.push_back(*r);
    m_AcceptorPimpl->apply_live();
    return 1;
}

void ServerSocket::SetMaxSimultaneousClients(int n) {
    m_AcceptorPimpl->policy.max_concurrent_connections =
        n > 0 ? static_cast<std::size_t>(n) : 0;
    m_AcceptorPimpl->apply_live();
}

void ServerSocket::DisconnectIfSilentFor(std::chrono::seconds t) {
    m_AcceptorPimpl->policy.idle_timeout = t;
    m_AcceptorPimpl->apply_live();
}

void ServerSocket::SetMaxMessageSizeBytes(std::size_t n) {
    m_AcceptorPimpl->policy.max_message_size = n;
    m_AcceptorPimpl->apply_live();
}

}  // namespace igtl
