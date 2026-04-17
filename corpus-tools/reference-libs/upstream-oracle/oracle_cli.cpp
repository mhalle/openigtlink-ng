// upstream_oracle_cli — wraps the pinned upstream OpenIGTLink C++
// reference library as a 6th oracle for `oigtl-corpus fuzz differential`.
//
// Wire protocol is identical to core-cpp/src/oracle_cli.cpp: one hex-
// encoded wire message per stdin line, one JSON report per stdout line:
//
//   {
//     "ok":            bool,
//     "type_id":       str,
//     "device_name":   str,
//     "version":       int,
//     "body_size":     int,
//     "ext_header_size": int | null,
//     "metadata_count": int,
//     "round_trip_ok": bool,
//     "error":         str
//   }
//
// Scope note (see security/PLAN.md Phase 5 closeout + the follow-up
// 6th-oracle plan): the runner feeds this CLI *well-formed* inputs
// only — inputs already accepted by another oracle. Upstream's readers
// predate modern sanitizers and are known to crash on adversarial
// input; process isolation turns any such crash into a clean
// subprocess exit that the runner treats as "upstream crashed" (not
// as a cross-codec disagreement).

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "igtlMessageBase.h"
#include "igtlMessageFactory.h"
#include "igtlMessageHeader.h"

// Classes upstream compiles but does *not* register in its default
// MessageFactory. Adding them explicitly closes the gap with our
// codec set so conformance checks aren't blocked by factory narrowness.
// (Discovered via the first oracle round on spec/corpus/upstream-fixtures:
// BIND / COLORT / NDARRAY / SENSOR were all reporting "no codec for
// this type_id" despite upstream shipping the implementations.)
#include "igtlBindMessage.h"
#include "igtlColorTableMessage.h"
#include "igtlNDArrayMessage.h"
#include "igtlSensorMessage.h"

namespace {

// ---------------------------------------------------------------------------
// Hex → bytes. Mirrors core-cpp/src/oracle_cli.cpp exactly.
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
// JSON string-escape. Non-ASCII and control bytes escape as \u00XX so
// the output is valid UTF-8 regardless of what bytes the input
// device_name / type_id carried. Same helper shape as the C++ oracle.
// ---------------------------------------------------------------------------
std::string json_escape(const std::string& s) {
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
// Report accumulator. Populated progressively; emitted once per
// input. Shape matches the other oracle CLIs exactly.
// ---------------------------------------------------------------------------
struct Report {
    bool ok = false;
    std::string type_id;
    std::string device_name;
    int version = 0;
    std::uint64_t body_size = 0;
    bool has_ext_header = false;
    unsigned ext_header_size = 0;
    std::size_t metadata_count = 0;
    bool round_trip_ok = false;
    std::string error;
};

void emit(const Report& r) {
    std::ostringstream os;
    os << "{";
    os << "\"ok\":" << (r.ok ? "true" : "false");
    os << ",\"type_id\":" << json_escape(r.type_id);
    os << ",\"device_name\":" << json_escape(r.device_name);
    os << ",\"version\":" << r.version;
    os << ",\"body_size\":" << r.body_size;
    if (r.has_ext_header) {
        os << ",\"ext_header_size\":" << r.ext_header_size;
    } else {
        os << ",\"ext_header_size\":null";
    }
    os << ",\"metadata_count\":" << r.metadata_count;
    os << ",\"round_trip_ok\":" << (r.round_trip_ok ? "true" : "false");
    os << ",\"error\":" << json_escape(r.error);
    os << "}";
    std::cout << os.str() << "\n";
    std::cout.flush();
}

// ---------------------------------------------------------------------------
// Core: feed *bytes* through upstream's MessageHeader + MessageFactory.
//
// Steps:
//   1. Copy 58 bytes into a fresh MessageHeader, call Unpack() → parses
//      header in-place and validates CRC is not performed (CRC only
//      validates on body Unpack).
//   2. Pull out the semantic fields for the report.
//   3. Dispatch via MessageFactory::CreateReceiveMessage. If the type
//      is unregistered upstream (e.g. "GET_*" variants not in its
//      default factory), emit ok=true with round_trip_ok=false and
//      move on — that's not a disagreement, it's just upstream being
//      narrower than our codecs.
//   4. For registered types, copy body bytes in and call Unpack(1)
//      (with CRC check). If the bitmask has UNPACK_BODY, re-Pack()
//      the message and compare byte-for-byte against the input.
//      Agreement → round_trip_ok=true.
//
// Any step that fails populates error and returns with ok=false.
// ---------------------------------------------------------------------------
void process(const std::vector<std::uint8_t>& bytes, Report& r) {
    constexpr std::size_t kHeaderSize = 58;
    if (bytes.size() < kHeaderSize) {
        r.error = "input shorter than 58-byte header";
        return;
    }

    // --- Header ---
    auto headerMsg = igtl::MessageHeader::New();
    headerMsg->InitPack();
    std::memcpy(headerMsg->GetPackPointer(), bytes.data(), kHeaderSize);
    int hr = headerMsg->Unpack();
    if (!(hr & igtl::MessageHeader::UNPACK_HEADER)) {
        r.error = "upstream MessageHeader::Unpack() rejected header";
        return;
    }
    r.type_id = headerMsg->GetDeviceType();
    r.device_name = headerMsg->GetDeviceName();
    r.version = headerMsg->GetHeaderVersion();
    r.body_size = headerMsg->GetBodySizeToRead();

    // --- Dispatch ---
    // Extend upstream's default factory with message types it ships
    // but doesn't auto-register. A static one-shot keeps this
    // amortized across all subsequent lines read from stdin.
    static igtl::MessageFactory::Pointer factory = [] {
        auto f = igtl::MessageFactory::New();
        f->AddMessageType("BIND",
            (igtl::MessageFactory::PointerToMessageBaseNew)
                &igtl::BindMessage::New);
        f->AddMessageType("COLORT",
            (igtl::MessageFactory::PointerToMessageBaseNew)
                &igtl::ColorTableMessage::New);
        f->AddMessageType("NDARRAY",
            (igtl::MessageFactory::PointerToMessageBaseNew)
                &igtl::NDArrayMessage::New);
        f->AddMessageType("SENSOR",
            (igtl::MessageFactory::PointerToMessageBaseNew)
                &igtl::SensorMessage::New);
        return f;
    }();
    igtl::MessageBase::Pointer msg = factory->CreateReceiveMessage(headerMsg);
    if (msg.IsNull()) {
        // Unregistered type — upstream is narrower than our codecs
        // (e.g. GET_BIND, GET_COLORT). Report header-only success so
        // the runner can still compare header-level fields; it will
        // see round_trip_ok=false and know upstream didn't attempt
        // body decode.
        r.ok = true;
        r.error = "upstream has no codec for this type_id";
        return;
    }

    // --- Body + round-trip ---
    const std::size_t body_start = kHeaderSize;
    const std::size_t body_needed = msg->GetPackBodySize();
    if (bytes.size() < body_start + body_needed) {
        r.error = "input truncated: header declares body_size="
                + std::to_string(body_needed) + " but only "
                + std::to_string(bytes.size() - body_start)
                + " body bytes available";
        return;
    }
    std::memcpy(msg->GetPackBodyPointer(), bytes.data() + body_start, body_needed);

    // Upstream quirk (igtlMessageBase::UnpackExtendedHeader): upstream
    // hardcodes ``sizeof(igtl_extended_header) == 12`` as the offset
    // where the per-message content begins, regardless of the
    // ext_header_size value carried on the wire. The spec allows
    // ``ext_header_size >= 12`` with the extra bytes reserved for
    // future fields. Our codecs advance to ``offset = ext_header_size``
    // as declared; upstream does not. Surface this distinctly so the
    // runner can filter it from the disagreement set — it's a real
    // upstream limitation, not a cross-codec bug we need to triage.
    if (r.version >= 2 && body_needed >= 2) {
        const std::uint16_t declared =
            (std::uint16_t(bytes[body_start]) << 8) | bytes[body_start + 1];
        if (declared != 12) {
            r.ok = true;
            r.error = "upstream quirk: ext_header_size != 12 not supported";
            return;
        }
    }

    int br = msg->Unpack(1);  // 1 = CRC check
    if (!(br & igtl::MessageHeader::UNPACK_BODY)) {
        // Upstream quirk (igtlMessageBase.cxx::Unpack, v1 path): the
        // call to UnpackBody is gated on ``GetBufferBodySize() > 0``,
        // so a v1 message with an empty body silently skips body
        // unpacking and the UNPACK_BODY flag never gets set. That's
        // "body unpack not attempted" rather than "rejected"; mark
        // it distinctly so the runner can filter it from the
        // disagreement set without treating it as a real reject.
        if (r.version < 2 && body_needed == 0) {
            r.ok = true;
            r.error = "upstream quirk: v1 empty body not unpacked";
            return;
        }
        r.error = "upstream body Unpack() rejected input";
        return;
    }

    // v2+ extended header + metadata — extract after body unpack.
    if (r.version >= 2) {
        r.has_ext_header = true;
        // Upstream exposes metadata_header_size + metadata_size but not
        // the 2-byte extended_header_size field directly via the public
        // API. The value is always 12 by the spec — this is a load-
        // bearing assumption the schema encodes. Record it as 12 so the
        // report compares cleanly against the other oracles.
        r.ext_header_size = 12;
        r.metadata_count = msg->GetMetaData().size();
    }

    // Round-trip: Pack() then compare.
    msg->Pack();
    const std::uint64_t repacked_size = msg->GetBufferSize();
    if (repacked_size == bytes.size()
        && std::memcmp(msg->GetBufferPointer(), bytes.data(), bytes.size()) == 0) {
        r.round_trip_ok = true;
    }
    r.ok = true;
}

}  // namespace

int main() {
    std::ios_base::sync_with_stdio(false);
    std::string line;
    std::vector<std::uint8_t> bytes;
    while (std::getline(std::cin, line)) {
        while (!line.empty()
               && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        Report r;
        if (!hex_to_bytes(line, bytes)) {
            r.error = "oracle_cli: invalid hex input";
            emit(r);
            continue;
        }
        process(bytes, r);
        emit(r);
    }
    return 0;
}
