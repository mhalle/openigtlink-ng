// Local-interface enumeration.
//
// POSIX backend via getifaddrs(3). For each interface with an IPv4
// or IPv6 address, produce one InterfaceAddress entry recording
// the subnet the interface lives on (computed from ip & netmask
// → first/last in an IpRange).
//
// Windows support pending: see TRANSPORT_PLAN.md. The Windows
// equivalent is GetAdaptersAddresses() from iphlpapi.lib. The
// shape of the returned data is the same; only the ingestion
// loop changes.

#include "oigtl/transport/policy.hpp"

#include <array>
#include <cstring>
#include <string>

#ifdef _WIN32
#error "net_iface is POSIX-only; Windows support is pending"
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace oigtl::transport {

namespace {

// Count how many leading 1-bits are set in `mask` (up to `bytes`).
// Used to convert a netmask byte array into a prefix length for
// the subnet-range computation.
std::size_t mask_prefix_length(const std::uint8_t* mask,
                               std::size_t bytes) {
    std::size_t bits = 0;
    for (std::size_t i = 0; i < bytes; ++i) {
        if (mask[i] == 0xFF) { bits += 8; continue; }
        if (mask[i] == 0x00) break;
        // Partial byte — count leading ones.
        std::uint8_t v = mask[i];
        while (v & 0x80) { ++bits; v = static_cast<std::uint8_t>(v << 1); }
        break;
    }
    return bits;
}

}  // namespace

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

            // Subnet = ip & mk, broadcast = ip | ~mk.
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
            // IPv4 link-local (APIPA): 169.254.0.0/16.
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
            // IPv6 link-local: fe80::/10.
            ia.is_link_local = (ip[0] == 0xfe &&
                                (ip[1] & 0xc0) == 0x80);
            (void)mask_prefix_length;   // kept for symmetry with future logging
            out.push_back(std::move(ia));
        }
        // Other families (packet sockets, AF_LINK on BSD) ignored.
    }

    return out;
}

}  // namespace oigtl::transport
