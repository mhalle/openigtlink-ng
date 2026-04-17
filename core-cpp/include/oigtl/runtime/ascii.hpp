// Strict-ASCII validation helpers shared by the generated codec.
//
// OpenIGTLink's spec defines type_id / device_name / fixed-string
// content fields as ASCII. Bytes >= 0x80 in these fields are always
// malformed — the reference Python codec rejects them via
// ``bytes.decode("ascii")``, and the differential fuzzer surfaced
// the C++ codec's prior permissive behaviour as a cross-language
// divergence class. Strict everywhere closes the class.
//
// These helpers are used by both the hand-written header parser and
// the generated per-message unpack code for ``fixed_string``,
// ``trailing_string``, and ``length_prefixed_string`` fields that
// declare ``encoding="ascii"``.

#ifndef OIGTL_RUNTIME_ASCII_HPP
#define OIGTL_RUNTIME_ASCII_HPP

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "oigtl/runtime/error.hpp"

namespace oigtl::runtime::ascii {

// Walk up to *size* bytes starting at *data*; stop at the first NUL
// or at *size*, whichever comes first. Every pre-NUL byte must be
// ASCII (< 0x80); throw MalformedMessageError on violation.
// Returns the prefix length (bytes consumed before NUL / end).
inline std::size_t null_padded_length(
    const std::uint8_t* data,
    std::size_t size,
    const char* field_name
) {
    std::size_t len = 0;
    while (len < size && data[len] != 0) {
        if (data[len] >= 0x80) {
            std::ostringstream oss;
            oss << "field \"" << field_name
                << "\" has non-ASCII byte 0x" << std::hex
                << static_cast<unsigned>(data[len])
                << " at offset " << std::dec << len;
            throw oigtl::error::MalformedMessageError(oss.str());
        }
        ++len;
    }
    return len;
}

// Check all *size* bytes starting at *data* are ASCII (< 0x80).
// Used for null_padded=false fixed-string fields where every byte
// is significant, and for length_prefixed / trailing strings.
// Throws MalformedMessageError on any non-ASCII byte.
inline void check_bytes(
    const std::uint8_t* data,
    std::size_t size,
    const char* field_name
) {
    for (std::size_t i = 0; i < size; ++i) {
        if (data[i] >= 0x80) {
            std::ostringstream oss;
            oss << "field \"" << field_name
                << "\" has non-ASCII byte 0x" << std::hex
                << static_cast<unsigned>(data[i])
                << " at offset " << std::dec << i;
            throw oigtl::error::MalformedMessageError(oss.str());
        }
    }
}

}  // namespace oigtl::runtime::ascii

#endif  // OIGTL_RUNTIME_ASCII_HPP
