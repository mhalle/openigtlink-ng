// Private platform-abstraction header for the transport layer.
//
// Not installed. Intended for our own src/transport/ and
// compat/src/ translation units. Public consumers (Slicer, PLUS,
// etc.) never see this file.
//
// Scope: wraps every POSIX-ism the transport code uses directly
// (bypassing asio) — sync send/recv, socket options with per-OS
// argument shapes, polling, interface enumeration, hostname
// resolution, and one-time Winsock init.
//
// Design mirrors modern Kitware practice (vtksys/cmsys pattern):
// one typedef + small API, two implementation files — POSIX in
// net_compat_posix.cpp, Winsock in net_compat_winsock.cpp. No
// `#ifdef` in the callers.

#ifndef OIGTL_TRANSPORT_DETAIL_NET_COMPAT_HPP
#define OIGTL_TRANSPORT_DETAIL_NET_COMPAT_HPP

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "oigtl/transport/policy.hpp"   // InterfaceAddress, IpRange

namespace oigtl::transport::detail {

// ---------------------------------------------------------------
// Types
// ---------------------------------------------------------------

#ifdef _WIN32
// Windows SOCKET is UINT_PTR — we model it as uintptr_t to keep
// the width correct on 64-bit Windows. Upstream OpenIGTLink stores
// descriptors as `int`, which is technically undefined on x64 but
// works because valid SOCKETs currently fit in int. We don't carry
// that wart forward.
using socket_t = std::uintptr_t;
inline constexpr socket_t invalid_socket = ~socket_t{0};  // INVALID_SOCKET
#else
using socket_t = int;
inline constexpr socket_t invalid_socket = -1;
#endif

// Byte-count return type for send/recv. `ssize_t` on POSIX;
// Winsock uses `int`. We promote to ptrdiff_t on both to avoid
// hiding truncation on Windows.
using byte_count_t = std::ptrdiff_t;

// ---------------------------------------------------------------
// Process-level init
// ---------------------------------------------------------------

// Refcounted Winsock init. Safe to call from multiple threads,
// matched by `uninitialize()` (ref goes back down; WSACleanup runs
// at 0 and at process exit). POSIX: no-op. Every caller that
// opens a descriptor should call this once before; we call it
// internally from send/recv/poll/bind paths so callers don't
// have to remember, but the free function is exposed for tests.
void ensure_initialized();

// ---------------------------------------------------------------
// Sync I/O
// ---------------------------------------------------------------

// Send all bytes or throw ConnectionClosedError. SIGPIPE is
// suppressed per-call (Linux: MSG_NOSIGNAL; macOS: SO_NOSIGPIPE
// set by suppress_sigpipe(); Windows: no SIGPIPE at all).
//
// On POSIX EAGAIN/EWOULDBLOCK, blocks via poll(POLLOUT) until the
// kernel buffer has space. Retries on EINTR.
//
// Takes raw bytes; caller is responsible for framing.
void safe_send_all(socket_t s,
                   const std::uint8_t* buf,
                   std::size_t len);

// Attempt to receive up to `len` bytes. Returns bytes written
// into `buf`. Returns 0 on peer-close (FIN). Throws on error.
// Retries on EINTR; returns -1 on EAGAIN/EWOULDBLOCK (caller
// decides to poll or give up).
byte_count_t safe_recv(socket_t s,
                       std::uint8_t* buf,
                       std::size_t len);

// ---------------------------------------------------------------
// Polling (one fd — what all our call sites need)
// ---------------------------------------------------------------

enum class PollFor { Readable, Writable };

// Wait for readiness. Returns:
//   - true           the descriptor is ready
//   - false          timed out (timeout_ms >= 0 only)
// Throws ConnectionClosedError on error. `timeout_ms < 0` blocks
// indefinitely.
bool poll_one(socket_t s, PollFor what, int timeout_ms);

// ---------------------------------------------------------------
// Socket options
// ---------------------------------------------------------------

// Set SO_RCVTIMEO using the OS's native representation. `ms == 0`
// means "no timeout" (clears the option).
void set_recv_timeout(socket_t s, std::chrono::milliseconds ms);

// POSIX with SO_NOSIGPIPE (macOS, FreeBSD) installs the option
// so a future `send()` on a closed peer doesn't kill the process.
// No-op on Linux (uses MSG_NOSIGNAL per-call) and Windows
// (no SIGPIPE at all).
void suppress_sigpipe(socket_t s);

// Enable TCP keepalive with tuned intervals. When the peer goes
// silent for `idle` seconds the kernel starts probing; after
// `count` unanswered probes spaced `interval` apart, the kernel
// closes the socket and subsequent reads return 0 (EOF).
//
// Platform mapping:
//   Linux:    SO_KEEPALIVE + TCP_KEEPIDLE + TCP_KEEPINTVL + TCP_KEEPCNT
//   macOS:    SO_KEEPALIVE + TCP_KEEPALIVE (seconds) + TCP_KEEPINTVL +
//             TCP_KEEPCNT
//   Windows:  SIO_KEEPALIVE_VALS (count isn't individually tunable
//             on Windows; we ignore it there, documented in the
//             implementation)
//
// Non-fatal: any setsockopt failure is silently swallowed. A
// working connection that can't have keepalive tuned is still a
// working connection — dying silently would be worse than having
// keepalive misconfigured.
void configure_keepalive(socket_t s,
                         std::chrono::seconds idle,
                         std::chrono::seconds interval,
                         int count);

// ---------------------------------------------------------------
// Close
// ---------------------------------------------------------------

// Idempotent close. Swallows errors; an unclosable socket is a
// kernel-table inconsistency we can't recover from anyway.
void close_socket(socket_t s);

// ---------------------------------------------------------------
// Address parsing / formatting
// ---------------------------------------------------------------

enum class Family { V4, V6 };

// Parse an IP literal (IPv4 or IPv6). Returns true + writes into
// `out`. 4 bytes for V4 (rest zero-padded); 16 bytes for V6.
bool parse_ip_literal(std::string_view text,
                      Family& family,
                      std::array<std::uint8_t, 16>& out);

// Render bytes as dotted-quad / colon-hex. 4 bytes for V4.
std::string format_ip(Family family,
                      const std::array<std::uint8_t, 16>& bytes);

// ---------------------------------------------------------------
// Hostname resolution
// ---------------------------------------------------------------

struct ResolvedAddress {
    Family family;
    std::array<std::uint8_t, 16> bytes{};
};

// Resolve a hostname to one or more addresses. Returns empty on
// failure; otherwise one entry per A / AAAA record.
std::vector<ResolvedAddress>
resolve_hostname(std::string_view host);

// ---------------------------------------------------------------
// Interface enumeration
// ---------------------------------------------------------------

// POSIX: getifaddrs. Windows: GetAdaptersAddresses. Returns one
// InterfaceAddress per (interface × address-family) pair, with
// is_loopback / is_link_local flagged.
std::vector<InterfaceAddress> enumerate_interfaces();

}  // namespace oigtl::transport::detail

#endif  // OIGTL_TRANSPORT_DETAIL_NET_COMPAT_HPP
