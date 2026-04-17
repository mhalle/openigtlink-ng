// Hand-written IMGMETA facade. Body: N × 260-byte elements.

#include "igtl/igtlImageMetaMessage.h"

#include <algorithm>
#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

namespace {
constexpr std::size_t kElementSize = 260;
constexpr std::size_t kLenName     = 64;
constexpr std::size_t kLenDevice   = 20;
constexpr std::size_t kLenModality = 32;
constexpr std::size_t kLenPName    = 64;
constexpr std::size_t kLenPID      = 64;

void fs_pack(std::uint8_t* dst, std::size_t n,
             const std::string& src) {
    std::memset(dst, 0, n);
    const std::size_t len = std::min(n, src.size());
    if (len > 0) std::memcpy(dst, src.data(), len);
}
std::string fs_unpack(const std::uint8_t* src, std::size_t n) {
    std::size_t len = 0;
    while (len < n && src[len] != '\0') ++len;
    return std::string(reinterpret_cast<const char*>(src), len);
}
}  // namespace

// ---------------- ImageMetaElement ----------------
ImageMetaElement::ImageMetaElement()
    : m_TimeStamp(0), m_ScalarType(0) {
    m_Size[0] = m_Size[1] = m_Size[2] = 0;
}

int ImageMetaElement::SetName(const char* n) {
    m_Name = n ? n : "";  return static_cast<int>(m_Name.size());
}
int ImageMetaElement::SetDeviceName(const char* d) {
    m_DeviceName = d ? d : "";
    return static_cast<int>(m_DeviceName.size());
}
int ImageMetaElement::SetModality(const char* m) {
    m_Modality = m ? m : "";
    return static_cast<int>(m_Modality.size());
}
int ImageMetaElement::SetPatientName(const char* p) {
    m_PatientName = p ? p : "";
    return static_cast<int>(m_PatientName.size());
}
int ImageMetaElement::SetPatientID(const char* p) {
    m_PatientID = p ? p : "";
    return static_cast<int>(m_PatientID.size());
}

void ImageMetaElement::SetTimeStamp(TimeStamp::Pointer& t) {
    m_TimeStamp = t.IsNotNull() ? t->GetTimeStampUint64() : 0;
}
void ImageMetaElement::GetTimeStamp(TimeStamp::Pointer& t) {
    if (t.IsNull()) t = TimeStamp::New();
    t->SetTime(m_TimeStamp);
}

void ImageMetaElement::SetSize(igtlUint16 s[3]) {
    m_Size[0] = s[0]; m_Size[1] = s[1]; m_Size[2] = s[2];
}
void ImageMetaElement::SetSize(igtlUint16 si, igtlUint16 sj,
                                igtlUint16 sk) {
    m_Size[0] = si; m_Size[1] = sj; m_Size[2] = sk;
}
void ImageMetaElement::GetSize(igtlUint16* s) {
    s[0] = m_Size[0]; s[1] = m_Size[1]; s[2] = m_Size[2];
}
void ImageMetaElement::GetSize(igtlUint16& si, igtlUint16& sj,
                                igtlUint16& sk) {
    si = m_Size[0]; sj = m_Size[1]; sk = m_Size[2];
}

// ---------------- ImageMetaMessage ----------------
ImageMetaMessage::ImageMetaMessage() {
    m_SendMessageType = "IMGMETA";
}

int ImageMetaMessage::AddImageMetaElement(ImageMetaElement::Pointer& e) {
    m_ImageMetaList.push_back(e);
    return static_cast<int>(m_ImageMetaList.size());
}
void ImageMetaMessage::ClearImageMetaElement() {
    m_ImageMetaList.clear();
}
int ImageMetaMessage::GetNumberOfImageMetaElement() {
    return static_cast<int>(m_ImageMetaList.size());
}
void ImageMetaMessage::GetImageMetaElement(
        int index, ImageMetaElement::Pointer& e) {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= m_ImageMetaList.size()) {
        e = ImageMetaElement::Pointer();
        return;
    }
    e = m_ImageMetaList[index];
}

igtlUint64 ImageMetaMessage::CalculateContentBufferSize() {
    return m_ImageMetaList.size() * kElementSize;
}

int ImageMetaMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t need = m_ImageMetaList.size() * kElementSize;
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();

    for (auto& e : m_ImageMetaList) {
        std::size_t off = 0;
        fs_pack(out + off, kLenName,     e->GetName());      off += kLenName;
        fs_pack(out + off, kLenDevice,   e->GetDeviceName()); off += kLenDevice;
        fs_pack(out + off, kLenModality, e->GetModality());   off += kLenModality;
        fs_pack(out + off, kLenPName,    e->GetPatientName()); off += kLenPName;
        fs_pack(out + off, kLenPID,      e->GetPatientID());   off += kLenPID;

        TimeStamp::Pointer ts; e->GetTimeStamp(ts);
        bo::write_be_u64(out + off, ts->GetTimeStampUint64());
        off += 8;

        igtlUint16 sz[3]; e->GetSize(sz);
        bo::write_be_u16(out + off + 0, sz[0]);
        bo::write_be_u16(out + off + 2, sz[1]);
        bo::write_be_u16(out + off + 4, sz[2]);
        off += 6;

        out[off++] = e->GetScalarType();
        out[off++] = 0;  // reserved

        out += kElementSize;
        (void)off;  // suppress unused-var warning if any path changes
    }
    return 1;
}

int ImageMetaMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t sz = m_Content.size();
    if (sz % kElementSize != 0) return 0;
    ClearImageMetaElement();
    const std::size_t n = sz / kElementSize;
    const auto* in = m_Content.data();
    for (std::size_t i = 0; i < n; ++i) {
        auto e = ImageMetaElement::New();
        std::size_t off = 0;
        e->SetName(fs_unpack(in + off, kLenName).c_str());         off += kLenName;
        e->SetDeviceName(fs_unpack(in + off, kLenDevice).c_str()); off += kLenDevice;
        e->SetModality(fs_unpack(in + off, kLenModality).c_str()); off += kLenModality;
        e->SetPatientName(fs_unpack(in + off, kLenPName).c_str()); off += kLenPName;
        e->SetPatientID(fs_unpack(in + off, kLenPID).c_str());     off += kLenPID;

        auto ts = TimeStamp::New();
        ts->SetTime(bo::read_be_u64(in + off)); off += 8;
        TimeStamp::Pointer tsp = ts;
        e->SetTimeStamp(tsp);

        igtlUint16 s[3];
        s[0] = bo::read_be_u16(in + off + 0);
        s[1] = bo::read_be_u16(in + off + 2);
        s[2] = bo::read_be_u16(in + off + 4);
        off += 6;
        e->SetSize(s);

        e->SetScalarType(in[off]); off += 2;  // skip reserved
        (void)off;

        m_ImageMetaList.push_back(e);
        in += kElementSize;
    }
    return 1;
}

}  // namespace igtl
