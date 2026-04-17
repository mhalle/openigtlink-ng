// Envelope helpers — timestamp conversion + metadata constructors.

#include "oigtl/envelope.hpp"

#include <cstring>

namespace oigtl {

namespace {

// Nanoseconds → 32-bit fraction of second. Uses 64-bit math to
// avoid loss of precision when shifting a sub-second ns count up
// by 32 bits.
constexpr std::uint32_t nanos_to_fraction(std::uint64_t ns_in_second) {
    return static_cast<std::uint32_t>(
        (ns_in_second << 32) / 1'000'000'000ULL);
}

constexpr std::uint64_t fraction_to_nanos(std::uint32_t fraction) {
    return (static_cast<std::uint64_t>(fraction) * 1'000'000'000ULL)
           >> 32;
}

}  // namespace

IgtlTimestamp to_igtl_timestamp(
    std::chrono::system_clock::time_point tp) {
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        tp.time_since_epoch()).count();
    if (ns < 0) ns = 0;  // negative timestamps map to epoch
    const std::uint64_t total = static_cast<std::uint64_t>(ns);
    const std::uint64_t sec = total / 1'000'000'000ULL;
    const std::uint64_t rem = total % 1'000'000'000ULL;
    return (sec << 32) | nanos_to_fraction(rem);
}

std::chrono::system_clock::time_point
from_igtl_timestamp(IgtlTimestamp t) {
    const std::uint64_t sec = t >> 32;
    const std::uint32_t frac = static_cast<std::uint32_t>(t & 0xFFFFFFFFULL);
    const std::uint64_t ns =
        sec * 1'000'000'000ULL + fraction_to_nanos(frac);
    // system_clock::time_point::duration is typically microseconds
    // on libc++ / nanoseconds on libstdc++ — cast through the
    // clock's own duration to stay portable.
    using D = std::chrono::system_clock::duration;
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<D>(
            std::chrono::nanoseconds(static_cast<std::int64_t>(ns))));
}

IgtlTimestamp now_igtl() {
    return to_igtl_timestamp(std::chrono::system_clock::now());
}

MetadataEntry make_text_metadata(
    std::string key, std::string value) {
    MetadataEntry e;
    e.key = std::move(key);
    e.value_encoding = 3;   // US-ASCII
    e.value.assign(value.begin(), value.end());
    return e;
}

MetadataEntry make_utf8_metadata(
    std::string key, std::string value) {
    MetadataEntry e;
    e.key = std::move(key);
    e.value_encoding = 106;  // UTF-8
    e.value.assign(value.begin(), value.end());
    return e;
}

MetadataEntry make_raw_metadata(
    std::string key,
    std::vector<std::uint8_t> value,
    std::uint16_t encoding) {
    MetadataEntry e;
    e.key = std::move(key);
    e.value_encoding = encoding;
    e.value = std::move(value);
    return e;
}

std::optional<std::string>
metadata_text(const Metadata& meta, std::string_view key) {
    for (const auto& e : meta) {
        if (e.key == key &&
            (e.value_encoding == 3 || e.value_encoding == 106)) {
            return std::string(e.value.begin(), e.value.end());
        }
    }
    return std::nullopt;
}

}  // namespace oigtl
