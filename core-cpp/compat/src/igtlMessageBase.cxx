// igtlMessageBase.cxx — routes Pack/Unpack through our runtime
// codec so the shim doesn't duplicate wire-format logic.

#include "igtl/igtlMessageBase.h"
#include "igtl/igtlMessageHeader.h"
#include "igtl/igtlTimeStamp.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

#include "oigtl/runtime/error.hpp"
#include "oigtl/runtime/extended_header.hpp"
#include "oigtl/runtime/header.hpp"
#include "oigtl/runtime/metadata.hpp"

namespace igtl {

// ---------------------------------------------------------------
// Construction / identity
// ---------------------------------------------------------------
MessageBase::MessageBase() {
    // Default v2 so `Pack()` emits an extended header by default;
    // matches upstream's default. Consumers wanting v1 call
    // SetHeaderVersion(1).
    m_HeaderVersion = 2;
    m_Wire.resize(oigtl::runtime::kHeaderSize, 0);
}

MessageBase::~MessageBase() = default;

MessageBase::Pointer MessageBase::New() {
    Pointer p = new MessageBase;
    p->UnRegister();
    return p;
}

// ---------------------------------------------------------------
// Simple setters / getters
// ---------------------------------------------------------------
void MessageBase::SetDeviceName(const char* n) {
    m_DeviceName = n ? std::string(n) : std::string();
}

void MessageBase::SetDeviceName(const std::string& n) {
    m_DeviceName = n;
}

std::string MessageBase::GetDeviceName() const { return m_DeviceName; }

int MessageBase::SetTimeStamp(unsigned int sec, unsigned int frac) {
    m_TimeStamp = (static_cast<igtlUint64>(sec) << 32) |
                  (static_cast<igtlUint64>(frac) & 0xFFFFFFFFu);
    return 1;
}

int MessageBase::GetTimeStamp(unsigned int* sec, unsigned int* frac) {
    if (sec)  *sec  = static_cast<unsigned int>(m_TimeStamp >> 32);
    if (frac) *frac = static_cast<unsigned int>(
        m_TimeStamp & 0xFFFFFFFFu);
    return 1;
}

void MessageBase::SetTimeStamp(SmartPointer<TimeStamp>& ts) {
    if (!ts) return;
    m_TimeStamp = ts->GetTimeStampUint64();
}

void MessageBase::GetTimeStamp(SmartPointer<TimeStamp>& ts) {
    if (!ts) return;
    ts->SetTime(m_TimeStamp);
}

// ---------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------
bool MessageBase::SetMetaDataElement(const std::string& key,
                                     IANA_ENCODING_TYPE encoding,
                                     std::string value) {
    m_MetaData[key] = MetaDataEntry{encoding, std::move(value)};
    return true;
}

bool MessageBase::GetMetaDataElement(const std::string& key,
                                     std::string& value) const {
    auto it = m_MetaData.find(key);
    if (it == m_MetaData.end()) return false;
    value = it->second.second;
    return true;
}

bool MessageBase::GetMetaDataElement(const std::string& key,
                                     IANA_ENCODING_TYPE& encoding,
                                     std::string& value) const {
    auto it = m_MetaData.find(key);
    if (it == m_MetaData.end()) return false;
    encoding = it->second.first;
    value = it->second.second;
    return true;
}

// ---------------------------------------------------------------
// Pack pipeline
// ---------------------------------------------------------------
int MessageBase::Pack() {
    namespace rt = oigtl::runtime;

    // 1) Ask subclass for the content size, then size m_Content and
    //    let it fill.
    const igtlUint64 content_size = CalculateContentBufferSize();
    m_Content.assign(static_cast<std::size_t>(content_size), 0);
    if (content_size > 0) {
        if (PackContent() != 1) return 0;
    }

    // 2) Build the body (content + optional ext-header + metadata).
    std::vector<std::uint8_t> body;
    if (m_HeaderVersion == 1) {
        body = m_Content;
    } else {
        // v2/v3: [12-byte ext-header][content][metadata]. Translate
        // our MetaDataMap into the codec's entry list.
        std::vector<rt::MetadataEntry> entries;
        entries.reserve(m_MetaData.size());
        for (const auto& kv : m_MetaData) {
            rt::MetadataEntry e;
            e.key = kv.first;
            e.value_encoding = kv.second.first;
            e.value.assign(kv.second.second.begin(),
                           kv.second.second.end());
            entries.push_back(std::move(e));
        }
        auto packed_meta = rt::pack_metadata(entries);

        rt::ExtendedHeader ext{};
        ext.ext_header_size = rt::kExtendedHeaderMinSize;
        ext.metadata_header_size = static_cast<std::uint16_t>(
            packed_meta.index_bytes.size());
        ext.metadata_size = static_cast<std::uint32_t>(
            packed_meta.body_bytes.size());
        ext.message_id = m_MessageID;

        body.resize(rt::kExtendedHeaderMinSize);
        rt::pack_extended_header(body.data(), ext);
        body.insert(body.end(), m_Content.begin(), m_Content.end());
        body.insert(body.end(),
                    packed_meta.index_bytes.begin(),
                    packed_meta.index_bytes.end());
        body.insert(body.end(),
                    packed_meta.body_bytes.begin(),
                    packed_meta.body_bytes.end());
    }

    // 3) Pack the 58-byte header with CRC over the body.
    std::array<std::uint8_t, rt::kHeaderSize> hdr{};
    rt::pack_header(hdr, m_HeaderVersion, m_SendMessageType,
                    m_DeviceName, m_TimeStamp,
                    body.data(), body.size());

    // 4) Concat into m_Wire.
    m_Wire.resize(hdr.size() + body.size());
    std::memcpy(m_Wire.data(), hdr.data(), hdr.size());
    if (!body.empty()) {
        std::memcpy(m_Wire.data() + hdr.size(),
                    body.data(), body.size());
    }
    m_BodySizeToRead = body.size();
    return 1;
}

#if 0  // superseded: logic inlined into Pack() above.
oigtl::runtime::PackedMetadata
MessageBase_pack_metadata(const MessageBase::MetaDataMap& m) {
    std::vector<oigtl::runtime::MetadataEntry> entries;
    entries.reserve(m.size());
    for (const auto& kv : m) {
        oigtl::runtime::MetadataEntry e;
        e.key = kv.first;
        e.value_encoding = kv.second.first;
        e.value.assign(kv.second.second.begin(),
                       kv.second.second.end());
        entries.push_back(std::move(e));
    }
    return oigtl::runtime::pack_metadata(entries);
}

// Give Pack() access to the above without polluting the public
// header with runtime::PackedMetadata.
oigtl::runtime::PackedMetadata pack_metadata_vec_impl(
        const MessageBase::MetaDataMap& m) {
    return MessageBase_pack_metadata(m);
}

#endif  // superseded

// ---------------------------------------------------------------
// Unpack pipeline
// ---------------------------------------------------------------
int MessageBase::Unpack(int crccheck) {
    namespace rt = oigtl::runtime;
    int status = UNPACK_UNDEF;

    if (m_Wire.size() < rt::kHeaderSize) return UNPACK_UNDEF;

    // 1) Parse the 58-byte outer header.
    rt::Header hdr;
    try {
        hdr = rt::unpack_header(m_Wire.data(), m_Wire.size());
    } catch (const std::exception&) {
        return UNPACK_UNDEF;
    }
    m_HeaderVersion   = hdr.version;
    m_SendMessageType = hdr.type_id;
    m_DeviceName      = hdr.device_name;
    m_TimeStamp       = hdr.timestamp;
    m_BodySizeToRead  = hdr.body_size;
    m_IsHeaderUnpacked = true;
    status |= UNPACK_HEADER;

    // 2) If the full body isn't present yet, stop after header.
    if (m_Wire.size() < rt::kHeaderSize + hdr.body_size) {
        return status;
    }

    // 3) Optional CRC check.
    if (crccheck) {
        try {
            rt::verify_crc(hdr, m_Wire.data() + rt::kHeaderSize,
                           hdr.body_size);
        } catch (const oigtl::error::CrcMismatchError&) {
            return status;
        }
    }

    // 4) Split body into content + metadata regions.
    const std::uint8_t* body = m_Wire.data() + rt::kHeaderSize;
    const std::size_t   body_len = hdr.body_size;

    if (hdr.version == 1) {
        m_Content.assign(body, body + body_len);
    } else {
        if (body_len < rt::kExtendedHeaderMinSize) return status;
        rt::ExtendedHeader ext;
        try {
            ext = rt::unpack_extended_header(body, body_len);
        } catch (const std::exception&) {
            return status;
        }
        m_MessageID = ext.message_id;

        const std::size_t hdr_size = ext.ext_header_size;
        const std::size_t meta_total =
            static_cast<std::size_t>(ext.metadata_header_size) +
            static_cast<std::size_t>(ext.metadata_size);
        if (body_len < hdr_size + meta_total) return status;
        const std::size_t content_size =
            body_len - hdr_size - meta_total;

        m_Content.assign(body + hdr_size,
                         body + hdr_size + content_size);

        if (meta_total > 0) {
            try {
                auto entries = rt::unpack_metadata(
                    body + hdr_size + content_size,
                    meta_total,
                    ext.metadata_header_size,
                    ext.metadata_size);
                m_MetaData.clear();
                for (const auto& e : entries) {
                    m_MetaData[e.key] = MetaDataEntry{
                        static_cast<IANA_ENCODING_TYPE>(e.value_encoding),
                        std::string(e.value.begin(), e.value.end())};
                }
            } catch (...) { /* leave metadata empty */ }
        }
    }

    // 5) Let subclass deserialise from m_Content.
    if (UnpackContent() == 1) {
        m_IsBodyUnpacked = true;
        status |= UNPACK_BODY;
    }
    return status;
}

// ---------------------------------------------------------------
// Raw-buffer accessors (pointer-stable until next Pack/Allocate)
// ---------------------------------------------------------------
void* MessageBase::GetBufferPointer() { return m_Wire.data(); }

void* MessageBase::GetBufferBodyPointer() {
    if (m_Wire.size() <= oigtl::runtime::kHeaderSize) return nullptr;
    return m_Wire.data() + oigtl::runtime::kHeaderSize;
}

igtl_uint64 MessageBase::GetBufferSize() {
    return static_cast<igtl_uint64>(m_Wire.size());
}

igtl_uint64 MessageBase::GetBufferBodySize() {
    if (m_Wire.size() <= oigtl::runtime::kHeaderSize) return 0;
    return static_cast<igtl_uint64>(
        m_Wire.size() - oigtl::runtime::kHeaderSize);
}

// ---------------------------------------------------------------
// Allocation
// ---------------------------------------------------------------
void MessageBase::InitBuffer() {
    m_Wire.assign(oigtl::runtime::kHeaderSize, 0);
    m_Content.clear();
    m_IsBodyUnpacked = false;
    m_IsHeaderUnpacked = false;
}

void MessageBase::AllocateBuffer() {
    // Resize, don't assign — preserves the 58-byte header the
    // caller has already deposited via InitPack+recv. Upstream's
    // AllocateBuffer does the same: it grows the buffer so a
    // body recv can land in wire[58..58+body_size].
    m_Wire.resize(
        oigtl::runtime::kHeaderSize +
            static_cast<std::size_t>(m_BodySizeToRead),
        0);
}

int MessageBase::SetMessageHeader(const MessageHeader* mh) {
    if (!mh) return 0;
    m_HeaderVersion    = mh->m_HeaderVersion;
    m_SendMessageType  = mh->m_SendMessageType;
    m_DeviceName       = mh->m_DeviceName;
    m_TimeStamp        = mh->m_TimeStamp;
    m_BodySizeToRead   = mh->m_BodySizeToRead;
    m_IsHeaderUnpacked = true;
    // Copy the 58-byte header into our wire buffer so a subsequent
    // CRC check (after body receive) has access.
    if (mh->m_Wire.size() >= oigtl::runtime::kHeaderSize) {
        m_Wire.assign(mh->m_Wire.begin(),
                      mh->m_Wire.begin() + oigtl::runtime::kHeaderSize);
    }
    return 1;
}

}  // namespace igtl

