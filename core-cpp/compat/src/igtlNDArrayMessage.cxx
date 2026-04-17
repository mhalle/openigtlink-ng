// Hand-written NDARRAY facade.

#include "igtl/igtlNDArrayMessage.h"

#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

namespace {
int element_bytes(int type) {
    switch (type) {
        case ArrayBase::TYPE_INT8:
        case ArrayBase::TYPE_UINT8:    return 1;
        case ArrayBase::TYPE_INT16:
        case ArrayBase::TYPE_UINT16:   return 2;
        case ArrayBase::TYPE_INT32:
        case ArrayBase::TYPE_UINT32:
        case ArrayBase::TYPE_FLOAT32:  return 4;
        case ArrayBase::TYPE_FLOAT64:  return 8;
        case ArrayBase::TYPE_COMPLEX:  return 8;  // 2 × float32
        default:                        return 0;
    }
}
}  // namespace

// ---------------- ArrayBase ----------------
ArrayBase::ArrayBase() = default;

int ArrayBase::SetSize(IndexType size) {
    m_Size = std::move(size);
    return 1;
}

igtlUint32 ArrayBase::GetNumberOfElements() {
    igtlUint32 n = 1;
    for (auto v : m_Size) n *= v;
    return n;
}

igtlUint64 ArrayBase::GetRawArraySize() {
    return static_cast<igtlUint64>(GetNumberOfElements()) *
           static_cast<igtlUint64>(GetElementSize());
}

int ArrayBase::SetArray(const void* src) {
    const std::size_t bytes = static_cast<std::size_t>(GetRawArraySize());
    m_ByteArray.assign(bytes, 0);
    if (src && bytes > 0) std::memcpy(m_ByteArray.data(), src, bytes);
    return 1;
}

// ---------------- NDArrayMessage ----------------
NDArrayMessage::NDArrayMessage()
    : m_Type(ArrayBase::TYPE_UINT8) {
    m_SendMessageType = "NDARRAY";
}

int NDArrayMessage::SetArray(int type, ArrayBase* a) {
    m_Type = type;
    m_Array = a;
    return 1;
}

igtlUint64 NDArrayMessage::CalculateContentBufferSize() {
    if (m_Array.IsNull()) return 2;
    const std::size_t dim = m_Array->GetSize().size();
    return 2 + 2 * dim + m_Array->GetRawArraySize();
}

int NDArrayMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    if (m_Array.IsNull()) return 0;
    const auto size = m_Array->GetSize();
    const auto dim = size.size();
    const std::size_t need = 2 + 2 * dim + m_Array->GetRawArraySize();
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();
    out[0] = static_cast<std::uint8_t>(m_Type);
    out[1] = static_cast<std::uint8_t>(dim);
    for (std::size_t i = 0; i < dim; ++i) {
        bo::write_be_u16(out + 2 + i * 2, size[i]);
    }
    const std::size_t data_off = 2 + 2 * dim;
    const std::size_t data_bytes = static_cast<std::size_t>(
        m_Array->GetRawArraySize());
    if (data_bytes > 0) {
        std::memcpy(out + data_off, m_Array->GetRawArray(), data_bytes);
    }
    return 1;
}

int NDArrayMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    if (m_Content.size() < 2) return 0;
    const auto* in = m_Content.data();
    const int type = in[0];
    const std::size_t dim = in[1];
    const std::size_t need_hdr = 2 + 2 * dim;
    if (m_Content.size() < need_hdr) return 0;

    // If no Array was set by the caller, synthesise a generic
    // typed Array based on `type`.
    if (m_Array.IsNull()) {
        switch (type) {
            case ArrayBase::TYPE_INT8:    m_Array = Array<std::int8_t>::New(); break;
            case ArrayBase::TYPE_UINT8:   m_Array = Array<std::uint8_t>::New(); break;
            case ArrayBase::TYPE_INT16:   m_Array = Array<std::int16_t>::New(); break;
            case ArrayBase::TYPE_UINT16:  m_Array = Array<std::uint16_t>::New(); break;
            case ArrayBase::TYPE_INT32:   m_Array = Array<std::int32_t>::New(); break;
            case ArrayBase::TYPE_UINT32:  m_Array = Array<std::uint32_t>::New(); break;
            case ArrayBase::TYPE_FLOAT32: m_Array = Array<float>::New();         break;
            case ArrayBase::TYPE_FLOAT64: m_Array = Array<double>::New();        break;
            default: return 0;
        }
    }
    m_Type = type;

    ArrayBase::IndexType size(dim);
    for (std::size_t i = 0; i < dim; ++i) {
        size[i] = bo::read_be_u16(in + 2 + i * 2);
    }
    m_Array->SetSize(size);

    const std::size_t data_bytes =
        m_Array->GetNumberOfElements() *
        static_cast<std::size_t>(element_bytes(type));
    if (m_Content.size() < need_hdr + data_bytes) return 0;
    m_Array->SetArray(in + need_hdr);
    return 1;
}

}  // namespace igtl
