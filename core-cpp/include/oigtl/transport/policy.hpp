// Accept-time policy for inbound connections.
//
// Every restriction is optional and composes additively. A default-
// constructed PeerPolicy accepts any peer — matching the pre-policy
// behaviour. This is deliberate: upstream `igtl::ServerSocket`
// consumers get no surprises on upgrade.
//
// Checks run at `accept()` time, before the first IGTL byte is
// read. A blocked peer gets a TCP RST within microseconds and
// cannot reach the framer or any UnpackContent() path, so the
// restrictions also serve as a pre-parse DoS mitigation layer.
//
// Researcher-friendly overview in core-cpp/compat/MIGRATION.md
// (section "Restricting which peers may connect"). That document
// is the one a non-network-engineer should read first; this
// header is the low-level transport API the compat shim calls
// into.
#ifndef OIGTL_TRANSPORT_POLICY_HPP
#define OIGTL_TRANSPORT_POLICY_HPP

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oigtl::transport {

// One IPv4 or IPv6 address range. Internal representation: two
// equal-family addresses as big-endian byte arrays, inclusive of
// both endpoints. A single address is a range with first == last.
//
// Parse via the factory functions below — callers never construct
// this struct directly.
struct IpRange {
    enum class Family : std::uint8_t { V4, V6 };
    Family family = Family::V4;
    // 4 bytes for V4, 16 bytes for V6. Unused bytes for V4 are zero.
    std::array<std::uint8_t, 16> first{};
    std::array<std::uint8_t, 16> last{};

    // Human-readable rendering of the range (e.g. "10.1.2.1 - 10.1.2.254"
    // or "fd00::1/64"). For logging only; no caller relies on exact format.
    std::string to_string() const;

    // Does this range contain the given peer address? `peer` must
    // be in the same byte format (4 or 16 bytes big-endian).
    bool contains(Family peer_family,
                  const std::array<std::uint8_t, 16>& peer) const;
};

// Parse a single IPv4 or IPv6 literal (e.g. "10.1.2.42", "::1").
// Returns nullopt on malformed input.
std::optional<IpRange> parse_ip(std::string_view ip);

// Parse CIDR notation (e.g. "10.1.2.0/24", "fd00::/8"). Returns
// nullopt on malformed input.
std::optional<IpRange> parse_cidr(std::string_view cidr);

// Parse a two-endpoint range (first and last inclusive). Both
// endpoints must be the same address family, and `first <= last`
// in big-endian byte order. Returns nullopt on malformed input.
std::optional<IpRange> parse_range(std::string_view first,
                                   std::string_view last);

// Parse any of the above forms. Accepts:
//   "10.1.2.42"          single host
//   "10.1.2.0/24"        CIDR
//   "10.1.2.1-10.1.2.254" range (dash-separated)
//   "10.1.2.1 - 10.1.2.254" range (spaces tolerated)
//   "::1"                IPv6 single host
//   "fd00::/8"           IPv6 CIDR
// Returns nullopt on malformed input. Hostnames are NOT resolved
// here — the caller is responsible for DNS lookup if it wants to
// accept a name.
std::optional<IpRange> parse(std::string_view spec);

// Snapshot of the host's network interfaces. One entry per
// address-family / interface pair (so a dual-stack interface
// produces two entries, one V4 and one V6).
struct InterfaceAddress {
    std::string name;       // "eth0", "en0", "Ethernet 2"
    IpRange    subnet;      // the address + mask, as a range
    bool is_loopback = false;
    bool is_link_local = false;
};

// Enumerate the local host's network interfaces. Filter rules
// applied:
//   - Loopback included (flagged is_loopback=true)
//   - IPv4 APIPA (169.254.0.0/16) flagged is_link_local=true
//   - IPv6 link-local (fe80::/10) flagged is_link_local=true
//   - Interfaces that are DOWN are skipped
//
// POSIX-only today (getifaddrs). Returns an empty vector on
// unsupported platforms. See the per-platform notes in
// core-cpp/src/transport/net_iface.cpp.
std::vector<InterfaceAddress> enumerate_interfaces();

// The full accept-time policy. All fields default to "no
// restriction". Consumers can mutate this and pass it to
// `tcp::listen(..., PeerPolicy)` at listen time or via
// `Acceptor::set_policy(...)` later.
struct PeerPolicy {
    // If empty, any peer is allowed. If non-empty, a peer is
    // accepted iff its address lies in at least one range.
    std::vector<IpRange> allowed_peers;

    // 0 = unlimited. If non-zero, connections already accepted
    // but not yet closed count against this cap. A new connection
    // arriving over the cap is rejected (accepted briefly then
    // closed).
    std::size_t max_concurrent_connections = 0;

    // 0 = no timeout. If non-zero, a Connection with no received
    // bytes for this duration is closed. Enforced in the sync
    // receive path via SO_RCVTIMEO.
    std::chrono::seconds idle_timeout{0};

    // 0 = no per-message cap (body_size is still bounded by the
    // 64-bit field itself, which is already checked). If non-zero,
    // any wire message with body_size > this value triggers a
    // FramingError and connection close before body bytes are
    // allocated.
    std::size_t max_message_size = 0;
};

}  // namespace oigtl::transport

#endif  // OIGTL_TRANSPORT_POLICY_HPP
