// IP-range parsing + address utilities.
//
// We deliberately avoid calling inet_pton/inet_ntop at the public
// boundary so the tests can exercise parse/contain logic without
// needing the platform's socket headers. The internal
// representation is a pair of big-endian byte arrays — identical
// layout to what `recvfrom`/`getpeername` hand us on the wire.

#include "oigtl/transport/policy.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>

#include "oigtl/transport/detail/net_compat.hpp"    // parse_ip_literal, format_ip

namespace oigtl::transport {

namespace {

// Big-endian 128-bit compare. Returns <0 if a<b, 0 if equal, >0 if a>b.
int cmp128(const std::array<std::uint8_t, 16>& a,
           const std::array<std::uint8_t, 16>& b) {
    for (std::size_t i = 0; i < 16; ++i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

bool parse_inet(std::string_view s, IpRange::Family& fam,
                std::array<std::uint8_t, 16>& out) {
    detail::Family df;
    if (!detail::parse_ip_literal(s, df, out)) return false;
    fam = (df == detail::Family::V4)
              ? IpRange::Family::V4
              : IpRange::Family::V6;
    return true;
}

// Produce a netmask of the given prefix length as a 16-byte array.
// For IPv4 the caller wants only the first 4 bytes; they're zero
// beyond that anyway.
void make_mask(std::size_t bits, std::array<std::uint8_t, 16>& out) {
    out.fill(0);
    for (std::size_t i = 0; i < 16 && bits > 0; ++i) {
        const std::size_t take = std::min<std::size_t>(8, bits);
        out[i] = static_cast<std::uint8_t>(0xFFu << (8 - take));
        bits -= take;
    }
}

std::string render_ip(IpRange::Family fam,
                      const std::array<std::uint8_t, 16>& bytes) {
    const auto df = (fam == IpRange::Family::V4)
                        ? detail::Family::V4
                        : detail::Family::V6;
    return detail::format_ip(df, bytes);
}

}  // namespace

// ---------------------------------------------------------------
// IpRange::contains / to_string
// ---------------------------------------------------------------

bool IpRange::contains(Family peer_family,
                       const std::array<std::uint8_t, 16>& peer) const {
    if (peer_family != family) return false;
    return cmp128(first, peer) <= 0 && cmp128(peer, last) <= 0;
}

std::string IpRange::to_string() const {
    if (cmp128(first, last) == 0) {
        return render_ip(family, first);
    }
    return render_ip(family, first) + " - " + render_ip(family, last);
}

// ---------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------

std::optional<IpRange> parse_ip(std::string_view ip) {
    ip = trim(ip);
    IpRange::Family fam;
    std::array<std::uint8_t, 16> bytes{};
    if (!parse_inet(ip, fam, bytes)) return std::nullopt;
    IpRange r;
    r.family = fam;
    r.first = bytes;
    r.last  = bytes;
    return r;
}

std::optional<IpRange> parse_cidr(std::string_view cidr) {
    cidr = trim(cidr);
    auto slash = cidr.find('/');
    if (slash == std::string_view::npos) return std::nullopt;
    auto addr_part = cidr.substr(0, slash);
    auto bits_part = cidr.substr(slash + 1);

    IpRange::Family fam;
    std::array<std::uint8_t, 16> base{};
    if (!parse_inet(addr_part, fam, base)) return std::nullopt;

    // Parse prefix length.
    std::string bits_s(bits_part);
    char* end = nullptr;
    const long bits_l = std::strtol(bits_s.c_str(), &end, 10);
    if (end == bits_s.c_str() || *end != '\0' || bits_l < 0) {
        return std::nullopt;
    }
    const std::size_t max_bits =
        (fam == IpRange::Family::V4) ? 32 : 128;
    if (static_cast<std::size_t>(bits_l) > max_bits) return std::nullopt;

    std::array<std::uint8_t, 16> mask{};
    make_mask(static_cast<std::size_t>(bits_l), mask);

    IpRange r;
    r.family = fam;
    // first = base & mask, last = base | ~mask.
    for (std::size_t i = 0; i < 16; ++i) {
        r.first[i] = base[i] & mask[i];
        r.last[i]  = base[i] | static_cast<std::uint8_t>(~mask[i]);
    }
    // Zero bytes beyond the family's width so the compare
    // semantics hold.
    if (fam == IpRange::Family::V4) {
        for (std::size_t i = 4; i < 16; ++i) {
            r.first[i] = 0;
            r.last[i]  = 0;
        }
    }
    return r;
}

std::optional<IpRange> parse_range(std::string_view first_s,
                                   std::string_view last_s) {
    IpRange::Family f1, f2;
    std::array<std::uint8_t, 16> a{}, b{};
    if (!parse_inet(trim(first_s), f1, a)) return std::nullopt;
    if (!parse_inet(trim(last_s),  f2, b)) return std::nullopt;
    if (f1 != f2) return std::nullopt;
    if (cmp128(a, b) > 0) return std::nullopt;
    IpRange r;
    r.family = f1;
    r.first = a;
    r.last  = b;
    return r;
}

// Public enumerate_interfaces() — thin facade over the detail
// platform abstraction. Lives here so compat/src/ and
// connection_tcp.cpp callers don't have to know about detail::.
std::vector<InterfaceAddress> enumerate_interfaces() {
    return detail::enumerate_interfaces();
}

std::optional<IpRange> parse(std::string_view spec) {
    spec = trim(spec);
    if (spec.empty()) return std::nullopt;

    // CIDR?
    if (spec.find('/') != std::string_view::npos) {
        return parse_cidr(spec);
    }

    // Dash-separated range? Split on '-' but be careful with
    // IPv6 ("::1" has no dashes; "fd00::1-fd00::ffff" has one).
    const auto dash = spec.find('-');
    if (dash != std::string_view::npos) {
        return parse_range(spec.substr(0, dash),
                           spec.substr(dash + 1));
    }

    // Single host.
    return parse_ip(spec);
}

}  // namespace oigtl::transport
