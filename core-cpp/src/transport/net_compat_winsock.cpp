// Winsock backend for oigtl::transport::detail::net_compat.
//
// Counterpart to net_compat_posix.cpp. One translation unit; all
// `#include <ws2tcpip.h>` / `<iphlpapi.h>` lives here. Callers
// include the abstract header only.
//
// Notes specific to Windows:
//   * `WSAStartup` is refcounted via `std::once_flag` for the
//     first-call guarantee plus a global sentinel dtor for
//     `WSACleanup` at process exit. Upstream OpenIGTLink calls
//     WSAStartup per-socket-create and never calls WSACleanup —
//     we don't carry that wart forward.
//   * Winsock has no SIGPIPE, so `suppress_sigpipe` is a no-op
//     and `safe_send_all` uses flags=0.
//   * `send`/`recv`/`WSAPoll` take `int` byte counts. We clamp
//     per-call to avoid overflow on huge buffers (callers send
//     whole messages; max_message_size caps this from above).
//   * `WSAGetLastError()` replaces `errno`. Translation to a
//     readable string goes through `FormatMessageA`.

#ifndef _WIN32
#error "net_compat_winsock.cpp being compiled for non-Windows — see CMakeLists"
#endif

// Order matters: winsock2.h must come before windows.h. The MSVC
// hygiene defines (NOMINMAX, WIN32_LEAN_AND_MEAN, _WIN32_WINNT)
// are set via CMake `target_compile_definitions`, not here — we
// don't want this TU to compile with subtly different flags than
// the rest of the library.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>    // SIO_KEEPALIVE_VALS + struct tcp_keepalive
#include <iphlpapi.h>
#include <windows.h>

#include "oigtl/transport/detail/net_compat.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

#include "oigtl/transport/errors.hpp"

namespace oigtl::transport::detail {

// ---------------------------------------------------------------
// init / close
// ---------------------------------------------------------------

namespace {

// Refcount of outstanding init calls. We bump once on the first
// `ensure_initialized`; a global sentinel decrements to trigger
// `WSACleanup` at process exit. Additional calls are free (no
// repeated WSAStartup) — Winsock's own refcount is per-call, but
// calling WSAStartup repeatedly is cheap and benign; we avoid it
// anyway for tidiness.
std::once_flag g_wsa_once;
std::atomic<bool> g_wsa_ok{false};

struct WsaSentinel {
    ~WsaSentinel() {
        if (g_wsa_ok.load()) ::WSACleanup();
    }
};
WsaSentinel g_wsa_sentinel;

// Translate a WSAGetLastError() code to a readable string for
// exception messages.
std::string wsa_strerror(int code) {
    char* msg = nullptr;
    const auto flags = FORMAT_MESSAGE_ALLOCATE_BUFFER
                     | FORMAT_MESSAGE_FROM_SYSTEM
                     | FORMAT_MESSAGE_IGNORE_INSERTS;
    const auto len = ::FormatMessageA(
        flags, nullptr, static_cast<DWORD>(code),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&msg), 0, nullptr);
    std::string out;
    if (msg && len > 0) {
        // Trim trailing \r\n that FormatMessage appends.
        std::size_t end = len;
        while (end > 0 && (msg[end - 1] == '\r' || msg[end - 1] == '\n' ||
                           msg[end - 1] == ' ' || msg[end - 1] == '.')) {
            --end;
        }
        out.assign(msg, end);
    } else {
        out = "WSA error " + std::to_string(code);
    }
    if (msg) ::LocalFree(msg);
    return out;
}

}  // namespace

void ensure_initialized() {
    std::call_once(g_wsa_once, [] {
        WSADATA data{};
        const int rc = ::WSAStartup(MAKEWORD(2, 2), &data);
        if (rc != 0) {
            throw ConnectionClosedError(
                std::string("WSAStartup: ") + wsa_strerror(rc));
        }
        g_wsa_ok.store(true);
    });
}

void close_socket(socket_t s) {
    if (s == invalid_socket) return;
    (void)::closesocket(static_cast<SOCKET>(s));
}

// ---------------------------------------------------------------
// sync I/O
// ---------------------------------------------------------------

namespace {

// Cap per-call byte count to INT_MAX. `send`/`recv` return `int`
// on Winsock; splitting large transfers is the caller's concern
// but we do the clamp here rather than trust the caller.
int clamp_len(std::size_t len) {
    constexpr std::size_t cap =
        static_cast<std::size_t>((std::numeric_limits<int>::max)());
    return static_cast<int>(len < cap ? len : cap);
}

}  // namespace

void safe_send_all(socket_t s,
                   const std::uint8_t* buf,
                   std::size_t len) {
    std::size_t off = 0;
    while (off < len) {
        const int chunk = clamp_len(len - off);
        const int n = ::send(static_cast<SOCKET>(s),
                             reinterpret_cast<const char*>(buf + off),
                             chunk, 0);
        if (n > 0) { off += static_cast<std::size_t>(n); continue; }
        if (n == SOCKET_ERROR) {
            const int err = ::WSAGetLastError();
            if (err == WSAEINTR) continue;
            if (err == WSAEWOULDBLOCK) {
                if (!poll_one(s, PollFor::Writable, -1)) {
                    throw ConnectionClosedError(
                        "WSAPoll(POLLOUT) timed out on blocking send");
                }
                continue;
            }
            throw ConnectionClosedError(
                std::string("send: ") + wsa_strerror(err));
        }
        // n == 0 on send is unexpected (not a closed-peer signal on
        // Winsock, unlike recv); treat as retry.
    }
}

byte_count_t safe_recv(socket_t s,
                       std::uint8_t* buf,
                       std::size_t len) {
    for (;;) {
        const int chunk = clamp_len(len);
        const int n = ::recv(static_cast<SOCKET>(s),
                             reinterpret_cast<char*>(buf),
                             chunk, 0);
        if (n >= 0) return static_cast<byte_count_t>(n);
        const int err = ::WSAGetLastError();
        if (err == WSAEINTR) continue;
        if (err == WSAEWOULDBLOCK) return -1;
        // WSAECONNRESET on Windows is the "peer closed abortively"
        // signal. Report as connection-closed so callers can treat
        // it the same as POSIX EPIPE/ECONNRESET.
        throw ConnectionClosedError(
            std::string("recv: ") + wsa_strerror(err));
    }
}

// ---------------------------------------------------------------
// poll
// ---------------------------------------------------------------

bool poll_one(socket_t s, PollFor what, int timeout_ms) {
    WSAPOLLFD pfd{};
    pfd.fd     = static_cast<SOCKET>(s);
    pfd.events = static_cast<SHORT>(
        (what == PollFor::Readable) ? POLLRDNORM : POLLWRNORM);
    for (;;) {
        const int r = ::WSAPoll(&pfd, 1, timeout_ms);
        if (r > 0) return true;
        if (r == 0) return false;
        const int err = ::WSAGetLastError();
        if (err == WSAEINTR) continue;
        throw ConnectionClosedError(
            std::string("WSAPoll: ") + wsa_strerror(err));
    }
}

// ---------------------------------------------------------------
// socket options
// ---------------------------------------------------------------

void set_recv_timeout(socket_t s, std::chrono::milliseconds ms) {
    const DWORD dw = static_cast<DWORD>(
        ms.count() < 0 ? 0 : ms.count());
    (void)::setsockopt(static_cast<SOCKET>(s),
                       SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char*>(&dw),
                       sizeof(dw));
}

void suppress_sigpipe(socket_t /*s*/) {
    // Winsock has no SIGPIPE.
}

void configure_keepalive(socket_t s,
                         std::chrono::seconds idle,
                         std::chrono::seconds interval,
                         int count) {
    /* Windows uses SIO_KEEPALIVE_VALS via WSAIoctl. The `count`
     * (number of probes) is not individually tunable — the kernel
     * defaults to its own retry policy, typically 10. We pass idle
     * + interval and ignore count. `struct tcp_keepalive` is
     * declared in <mstcpip.h>. */
    (void)count;
    struct tcp_keepalive ka{};
    ka.onoff             = 1;
    ka.keepalivetime     = static_cast<ULONG>(
        std::chrono::duration_cast<std::chrono::milliseconds>(idle)
            .count());
    ka.keepaliveinterval = static_cast<ULONG>(
        std::chrono::duration_cast<std::chrono::milliseconds>(interval)
            .count());

    DWORD bytes_returned = 0;
    (void)::WSAIoctl(static_cast<SOCKET>(s),
                     SIO_KEEPALIVE_VALS,
                     &ka, sizeof ka,
                     nullptr, 0,
                     &bytes_returned,
                     nullptr, nullptr);
}

// ---------------------------------------------------------------
// address parsing
// ---------------------------------------------------------------

bool parse_ip_literal(std::string_view text,
                      Family& family,
                      std::array<std::uint8_t, 16>& out) {
    ensure_initialized();    // inet_pton needs Winsock init

    std::string tmp(text);
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
    ensure_initialized();
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
    ensure_initialized();

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

namespace {

// Convert IP_ADAPTER_ADDRESSES FriendlyName (wide) to UTF-8.
// Good-enough conversion for ASCII-range names; for fancy
// characters WideCharToMultiByte would be more correct, but the
// point is to give the user a recognisable identifier, and every
// realistic adapter name is ASCII.
std::string friendly_name(const PWCHAR w) {
    if (!w) return {};
    const int wlen = static_cast<int>(::wcslen(w));
    if (wlen == 0) return {};
    const int nlen = ::WideCharToMultiByte(
        CP_UTF8, 0, w, wlen, nullptr, 0, nullptr, nullptr);
    if (nlen <= 0) return {};
    std::string out(static_cast<std::size_t>(nlen), '\0');
    ::WideCharToMultiByte(
        CP_UTF8, 0, w, wlen, out.data(), nlen, nullptr, nullptr);
    return out;
}

// Build a mask array of `prefix` on-bits. Matches make_mask in
// policy.cpp but local to this TU — GetAdaptersAddresses returns
// OnLinkPrefixLength, which we need to turn into a netmask.
void make_prefix_mask(std::size_t prefix,
                      std::array<std::uint8_t, 16>& out) {
    out.fill(0);
    for (std::size_t i = 0; i < 16 && prefix > 0; ++i) {
        const std::size_t take = std::min<std::size_t>(8, prefix);
        out[i] = static_cast<std::uint8_t>(0xFFu << (8 - take));
        prefix -= take;
    }
}

}  // namespace

std::vector<InterfaceAddress> enumerate_interfaces() {
    ensure_initialized();
    std::vector<InterfaceAddress> out;

    // GetAdaptersAddresses wants a buffer. We grow on ERROR_BUFFER_OVERFLOW.
    ULONG size = 16 * 1024;
    std::vector<std::uint8_t> buf(size);
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST
                      | GAA_FLAG_SKIP_MULTICAST
                      | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG rc = ERROR_BUFFER_OVERFLOW;
    for (int attempt = 0; attempt < 4 && rc == ERROR_BUFFER_OVERFLOW; ++attempt) {
        buf.resize(size);
        rc = ::GetAdaptersAddresses(
            AF_UNSPEC, flags, nullptr,
            reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data()),
            &size);
    }
    if (rc != NO_ERROR) return out;

    for (auto* adapter =
             reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
         adapter != nullptr; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) continue;

        const bool is_lo =
            (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK);
        const std::string name = friendly_name(adapter->FriendlyName);

        for (auto* ua = adapter->FirstUnicastAddress; ua; ua = ua->Next) {
            const auto* sa = ua->Address.lpSockaddr;
            if (!sa) continue;

            if (sa->sa_family == AF_INET) {
                const auto* sin =
                    reinterpret_cast<const sockaddr_in*>(sa);
                std::array<std::uint8_t, 4> ip{};
                std::memcpy(ip.data(), &sin->sin_addr, 4);

                std::array<std::uint8_t, 16> mask16{};
                make_prefix_mask(ua->OnLinkPrefixLength, mask16);

                IpRange r;
                r.family = IpRange::Family::V4;
                for (std::size_t i = 0; i < 4; ++i) {
                    r.first[i] = static_cast<std::uint8_t>(
                        ip[i] & mask16[i]);
                    r.last[i] = static_cast<std::uint8_t>(
                        ip[i] | static_cast<std::uint8_t>(~mask16[i]));
                }

                InterfaceAddress ia;
                ia.name = name;
                ia.subnet = r;
                ia.is_loopback = is_lo;
                ia.is_link_local = (ip[0] == 169 && ip[1] == 254);
                out.push_back(std::move(ia));
            } else if (sa->sa_family == AF_INET6) {
                const auto* sin6 =
                    reinterpret_cast<const sockaddr_in6*>(sa);
                std::array<std::uint8_t, 16> ip{};
                std::memcpy(ip.data(), &sin6->sin6_addr, 16);

                std::array<std::uint8_t, 16> mask16{};
                make_prefix_mask(ua->OnLinkPrefixLength, mask16);

                IpRange r;
                r.family = IpRange::Family::V6;
                for (std::size_t i = 0; i < 16; ++i) {
                    r.first[i] = static_cast<std::uint8_t>(
                        ip[i] & mask16[i]);
                    r.last[i] = static_cast<std::uint8_t>(
                        ip[i] | static_cast<std::uint8_t>(~mask16[i]));
                }

                InterfaceAddress ia;
                ia.name = name;
                ia.subnet = r;
                ia.is_loopback = is_lo;
                ia.is_link_local = (ip[0] == 0xfe &&
                                    (ip[1] & 0xc0) == 0x80);
                out.push_back(std::move(ia));
            }
        }
    }

    return out;
}

}  // namespace oigtl::transport::detail
