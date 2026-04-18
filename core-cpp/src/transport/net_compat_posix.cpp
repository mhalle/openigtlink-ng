// POSIX backend for oigtl::transport::detail::net_compat.
//
// One translation unit, one platform. All the `#include <sys/*>`
// lives here. Callers include the abstract header only.

#ifdef _WIN32
// Defensive: this file is POSIX-only. The Windows build selects
// net_compat_winsock.cpp via CMake.
#error "net_compat_posix.cpp being compiled for Windows — see CMakeLists"
#endif

#include "oigtl/transport/detail/net_compat.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "oigtl/transport/errors.hpp"

namespace oigtl::transport::detail {

// ---------------------------------------------------------------
// init / close
// ---------------------------------------------------------------

void ensure_initialized() {
    // Nothing to do on POSIX. Winsock backend uses std::once_flag
    // + refcount here.
}

void close_socket(socket_t s) {
    if (s == invalid_socket) return;
    ::close(s);
}

// ---------------------------------------------------------------
// sync I/O
// ---------------------------------------------------------------

namespace {

int send_flags() {
#ifdef MSG_NOSIGNAL
    return MSG_NOSIGNAL;    // Linux
#else
    return 0;               // macOS / *BSD (SO_NOSIGPIPE on socket)
#endif
}

}  // namespace

void safe_send_all(socket_t s,
                   const std::uint8_t* buf,
                   std::size_t len) {
    const int flags = send_flags();
    std::size_t off = 0;
    while (off < len) {
        const auto n = ::send(s, buf + off, len - off, flags);
        if (n > 0) { off += static_cast<std::size_t>(n); continue; }
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (!poll_one(s, PollFor::Writable, -1)) {
                    // -1 timeout → poll_one returns only on ready
                    // or error; "false" would be a bug.
                    throw ConnectionClosedError(
                        "poll(POLLOUT) returned timeout on blocking");
                }
                continue;
            }
            throw ConnectionClosedError(
                std::string("send: ") + std::strerror(errno));
        }
    }
}

byte_count_t safe_recv(socket_t s,
                       std::uint8_t* buf,
                       std::size_t len) {
    for (;;) {
        const auto n = ::recv(s, buf, len, 0);
        if (n >= 0) return static_cast<byte_count_t>(n);
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;
        throw ConnectionClosedError(
            std::string("recv: ") + std::strerror(errno));
    }
}

// ---------------------------------------------------------------
// poll
// ---------------------------------------------------------------

bool poll_one(socket_t s, PollFor what, int timeout_ms) {
    struct pollfd pfd{};
    pfd.fd     = s;
    pfd.events = (what == PollFor::Readable) ? POLLIN : POLLOUT;
    for (;;) {
        const int r = ::poll(&pfd, 1, timeout_ms);
        if (r > 0) return true;
        if (r == 0) return false;    // timeout
        if (errno == EINTR) continue;
        throw ConnectionClosedError(
            std::string("poll: ") + std::strerror(errno));
    }
}

// ---------------------------------------------------------------
// socket options
// ---------------------------------------------------------------

void set_recv_timeout(socket_t s, std::chrono::milliseconds ms) {
    struct timeval tv{};
    const auto secs = ms.count() / 1000;
    tv.tv_sec  = static_cast<decltype(tv.tv_sec)>(secs);
    tv.tv_usec = static_cast<decltype(tv.tv_usec)>(
        (ms.count() - secs * 1000) * 1000);
    (void)::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

void suppress_sigpipe(socket_t s) {
#ifdef SO_NOSIGPIPE
    int on = 1;
    (void)::setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#else
    (void)s;    // Linux: MSG_NOSIGNAL per-send covers it.
#endif
}

void configure_keepalive(socket_t s,
                         std::chrono::seconds idle,
                         std::chrono::seconds interval,
                         int count) {
    int on = 1;
    (void)::setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

    const int idle_s     = static_cast<int>(idle.count());
    const int interval_s = static_cast<int>(interval.count());

#if defined(TCP_KEEPIDLE)     // Linux, FreeBSD >= 9.0
    (void)::setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE,
                       &idle_s, sizeof(idle_s));
#elif defined(TCP_KEEPALIVE)  // macOS: TCP_KEEPALIVE is "idle seconds"
    (void)::setsockopt(s, IPPROTO_TCP, TCP_KEEPALIVE,
                       &idle_s, sizeof(idle_s));
#else
    (void)idle_s;
#endif

#ifdef TCP_KEEPINTVL
    (void)::setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL,
                       &interval_s, sizeof(interval_s));
#else
    (void)interval_s;
#endif

#ifdef TCP_KEEPCNT
    (void)::setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT,
                       &count, sizeof(count));
#else
    (void)count;
#endif
}

// ---------------------------------------------------------------
// address parsing
// ---------------------------------------------------------------

bool parse_ip_literal(std::string_view text,
                      Family& family,
                      std::array<std::uint8_t, 16>& out) {
    std::string tmp(text);    // inet_pton needs a nul-terminated string
    out.fill(0);

    std::array<std::uint8_t, 4> v4{};
    if (::inet_pton(AF_INET, tmp.c_str(), v4.data()) == 1) {
        family = Family::V4;
        std::memcpy(out.data(), v4.data(), 4);
        return true;
    }
    std::array<std::uint8_t, 16> v6{};
    if (::inet_pton(AF_INET6, tmp.c_str(), v6.data()) == 1) {
        family = Family::V6;
        out = v6;
        return true;
    }
    return false;
}

std::string format_ip(Family family,
                      const std::array<std::uint8_t, 16>& bytes) {
    char buf[INET6_ADDRSTRLEN] = {0};
    const int af = (family == Family::V4) ? AF_INET : AF_INET6;
    (void)::inet_ntop(af, bytes.data(), buf, sizeof(buf));
    return std::string(buf);
}

// ---------------------------------------------------------------
// hostname resolution
// ---------------------------------------------------------------

std::vector<ResolvedAddress>
resolve_hostname(std::string_view host) {
    std::vector<ResolvedAddress> out;
    std::string tmp(host);

    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (::getaddrinfo(tmp.c_str(), nullptr, &hints, &res) != 0) {
        return out;
    }
    for (auto* p = res; p; p = p->ai_next) {
        ResolvedAddress r{};
        if (p->ai_family == AF_INET) {
            r.family = Family::V4;
            auto* sin = reinterpret_cast<sockaddr_in*>(p->ai_addr);
            std::memcpy(r.bytes.data(), &sin->sin_addr, 4);
            out.push_back(r);
        } else if (p->ai_family == AF_INET6) {
            r.family = Family::V6;
            auto* sin6 = reinterpret_cast<sockaddr_in6*>(p->ai_addr);
            std::memcpy(r.bytes.data(), &sin6->sin6_addr, 16);
            out.push_back(r);
        }
    }
    ::freeaddrinfo(res);
    return out;
}

// ---------------------------------------------------------------
// interface enumeration
// ---------------------------------------------------------------

std::vector<InterfaceAddress> enumerate_interfaces() {
    std::vector<InterfaceAddress> out;

    ifaddrs* head = nullptr;
    if (::getifaddrs(&head) != 0) return out;
    struct Cleanup {
        ifaddrs* p;
        ~Cleanup() { if (p) ::freeifaddrs(p); }
    } cleanup{head};

    for (ifaddrs* cur = head; cur != nullptr; cur = cur->ifa_next) {
        if (!cur->ifa_addr) continue;
        if (!(cur->ifa_flags & IFF_UP)) continue;

        const int fam = cur->ifa_addr->sa_family;
        const bool is_lo = (cur->ifa_flags & IFF_LOOPBACK) != 0;

        if (fam == AF_INET) {
            if (!cur->ifa_netmask) continue;
            const auto* sin_ip =
                reinterpret_cast<const sockaddr_in*>(cur->ifa_addr);
            const auto* sin_mk =
                reinterpret_cast<const sockaddr_in*>(cur->ifa_netmask);

            std::array<std::uint8_t, 4> ip{}, mk{};
            std::memcpy(ip.data(), &sin_ip->sin_addr, 4);
            std::memcpy(mk.data(), &sin_mk->sin_addr, 4);

            IpRange r;
            r.family = IpRange::Family::V4;
            for (std::size_t i = 0; i < 4; ++i) {
                r.first[i] = static_cast<std::uint8_t>(ip[i] & mk[i]);
                r.last[i]  = static_cast<std::uint8_t>(
                    ip[i] | static_cast<std::uint8_t>(~mk[i]));
            }

            InterfaceAddress ia;
            ia.name = cur->ifa_name ? cur->ifa_name : "";
            ia.subnet = r;
            ia.is_loopback = is_lo;
            ia.is_link_local = (ip[0] == 169 && ip[1] == 254);
            out.push_back(std::move(ia));
        } else if (fam == AF_INET6) {
            if (!cur->ifa_netmask) continue;
            const auto* sin_ip =
                reinterpret_cast<const sockaddr_in6*>(cur->ifa_addr);
            const auto* sin_mk =
                reinterpret_cast<const sockaddr_in6*>(cur->ifa_netmask);

            std::array<std::uint8_t, 16> ip{}, mk{};
            std::memcpy(ip.data(), &sin_ip->sin6_addr, 16);
            std::memcpy(mk.data(), &sin_mk->sin6_addr, 16);

            IpRange r;
            r.family = IpRange::Family::V6;
            for (std::size_t i = 0; i < 16; ++i) {
                r.first[i] = static_cast<std::uint8_t>(ip[i] & mk[i]);
                r.last[i]  = static_cast<std::uint8_t>(
                    ip[i] | static_cast<std::uint8_t>(~mk[i]));
            }

            InterfaceAddress ia;
            ia.name = cur->ifa_name ? cur->ifa_name : "";
            ia.subnet = r;
            ia.is_loopback = is_lo;
            ia.is_link_local = (ip[0] == 0xfe &&
                                (ip[1] & 0xc0) == 0x80);
            out.push_back(std::move(ia));
        }
    }

    return out;
}

}  // namespace oigtl::transport::detail
