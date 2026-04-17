// oigtl_oracle_cli — persistent oracle subprocess for differential fuzzing.
//
// Reads hex-encoded wire-byte strings from stdin (one per line) and
// emits a JSON oracle report to stdout for each. EOF terminates
// cleanly. Each iteration runs the full framing-aware oracle
// (`verify_wire_bytes`) and serializes the result shape shared with
// the Python / TS oracles:
//
//   {
//     "ok": bool,
//     "type_id": str,
//     "device_name": str,
//     "version": int,
//     "body_size": int,
//     "ext_header_size": int | null,
//     "metadata_count": int,
//     "round_trip_ok": bool,
//     "error": str
//   }
//
// The JSON is compact (no pretty-printing, no spaces) so the Python
// side can split on newlines and `json.loads` each line without
// reassembly. Stdout is line-buffered via explicit fflush.
//
// Used by `oigtl-corpus fuzz differential` to compare oracle
// outputs across implementations. Not for human consumption.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "oigtl/messages/register_all.hpp"
#include "oigtl/runtime/oracle.hpp"

namespace {

// ---------------------------------------------------------------------------
// Hex → bytes — lenient parse. Unexpected characters raise; odd-length
// inputs raise. No whitespace tolerated — the protocol uses newline
// as the record separator, so anything else in the line is an error.
// ---------------------------------------------------------------------------

bool hex_to_bytes(const std::string& hex, std::vector<std::uint8_t>& out) {
    if (hex.size() % 2 != 0) return false;
    out.clear();
    out.reserve(hex.size() / 2);
    auto nibble = [](char c, int& v) -> bool {
        if (c >= '0' && c <= '9') { v = c - '0'; return true; }
        if (c >= 'a' && c <= 'f') { v = c - 'a' + 10; return true; }
        if (c >= 'A' && c <= 'F') { v = c - 'A' + 10; return true; }
        return false;
    };
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        int hi = 0, lo = 0;
        if (!nibble(hex[i], hi) || !nibble(hex[i + 1], lo)) return false;
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return true;
}

// ---------------------------------------------------------------------------
// JSON escaping — minimal. The error strings we produce are ASCII
// apart from a single `\u00a7` (§) from the prose spec references.
// Handle the handful of JSON special chars inline.
// ---------------------------------------------------------------------------

std::string json_escape(const std::string& s) {
    // OpenIGTLink type_id / device_name fields are ASCII per spec,
    // but the fuzzer feeds arbitrary bytes. Escape every non-ASCII
    // byte as `\u00XX` so the JSON output is valid UTF-8 even when
    // the input device_name was e.g. 0xCB. Byte values map 1:1 to
    // Latin-1 code points, which is semantically arbitrary but
    // round-trips deterministically across the three oracle CLIs.
    std::string out;
    out.reserve(s.size() + 4);
    out.push_back('"');
    for (char c : s) {
        const auto uc = static_cast<unsigned char>(c);
        switch (c) {
            case '"':  out.append("\\\""); continue;
            case '\\': out.append("\\\\"); continue;
            case '\b': out.append("\\b");  continue;
            case '\f': out.append("\\f");  continue;
            case '\n': out.append("\\n");  continue;
            case '\r': out.append("\\r");  continue;
            case '\t': out.append("\\t");  continue;
            default: break;
        }
        if (uc < 0x20 || uc >= 0x7f) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", uc);
            out.append(buf);
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

// ---------------------------------------------------------------------------
// Emit one report line.
// ---------------------------------------------------------------------------

void emit_report(const oigtl::runtime::oracle::VerifyResult& r) {
    std::ostringstream os;
    os << "{";
    os << "\"ok\":" << (r.ok ? "true" : "false");
    os << ",\"type_id\":" << json_escape(r.header.type_id);
    os << ",\"device_name\":" << json_escape(r.header.device_name);
    os << ",\"version\":" << static_cast<unsigned>(r.header.version);
    os << ",\"body_size\":" << static_cast<unsigned long long>(r.header.body_size);
    if (r.extended_header.has_value()) {
        os << ",\"ext_header_size\":"
           << static_cast<unsigned>(r.extended_header->ext_header_size);
    } else {
        os << ",\"ext_header_size\":null";
    }
    os << ",\"metadata_count\":" << r.metadata.size();
    os << ",\"round_trip_ok\":" << (r.round_trip_ok ? "true" : "false");
    os << ",\"error\":" << json_escape(r.error);
    os << "}";
    std::cout << os.str() << "\n";
}

}  // namespace

int main() {
    // Disable any stdio synchronization shenanigans so one byte in →
    // one line out latency is predictable.
    std::ios_base::sync_with_stdio(false);

    const auto registry = oigtl::messages::default_registry();
    std::string line;
    std::vector<std::uint8_t> bytes;

    while (std::getline(std::cin, line)) {
        // Strip trailing \r from Windows line endings (defensive —
        // fuzzer is Python so won't emit these, but robust anyway).
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        if (!hex_to_bytes(line, bytes)) {
            oigtl::runtime::oracle::VerifyResult r;
            r.ok = false;
            r.error = "oracle_cli: invalid hex input";
            emit_report(r);
            std::cout.flush();
            continue;
        }
        const auto result = oigtl::runtime::oracle::verify_wire_bytes(
            bytes.data(), bytes.size(), registry, /*check_crc=*/true);
        emit_report(result);
        std::cout.flush();
    }
    return 0;
}
