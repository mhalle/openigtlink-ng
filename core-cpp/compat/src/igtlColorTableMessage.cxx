// Hand-written COLORT facade.

#include "igtl/igtlColorTableMessage.h"

#include <cstring>

namespace igtl {

namespace {
std::size_t entries_for(int index_type) {
    switch (index_type) {
        case ColorTableMessage::INDEX_UINT8:  return 256;
        case ColorTableMessage::INDEX_UINT16: return 65536;
        default:                              return 0;
    }
}
std::size_t bytes_per_entry(int map_type) {
    switch (map_type) {
        case ColorTableMessage::MAP_UINT8:  return 1;
        case ColorTableMessage::MAP_UINT16: return 2;
        case ColorTableMessage::MAP_RGB:    return 3;
        default:                            return 0;
    }
}
}  // namespace

ColorTableMessage::ColorTableMessage()
    : indexType(INDEX_UINT8), mapType(MAP_UINT8) {
    m_SendMessageType = "COLORT";
}

int ColorTableMessage::GetColorTableSize() {
    return static_cast<int>(
        entries_for(indexType) * bytes_per_entry(mapType));
}

void* ColorTableMessage::GetTablePointer() {
    return m_Table.data();
}

void ColorTableMessage::AllocateTable() {
    m_Table.assign(
        static_cast<std::size_t>(GetColorTableSize()), 0);
}

igtlUint64 ColorTableMessage::CalculateContentBufferSize() {
    return 2 + static_cast<igtlUint64>(GetColorTableSize());
}

int ColorTableMessage::PackContent() {
    const std::size_t tsize =
        static_cast<std::size_t>(GetColorTableSize());
    const std::size_t need = 2 + tsize;
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();
    out[0] = static_cast<std::uint8_t>(indexType);
    out[1] = static_cast<std::uint8_t>(mapType);
    if (tsize > 0 && !m_Table.empty()) {
        std::memcpy(out + 2, m_Table.data(),
                    std::min(tsize, m_Table.size()));
    }
    return 1;
}

int ColorTableMessage::UnpackContent() {
    if (m_Content.size() < 2) return 0;
    const auto* in = m_Content.data();
    indexType = in[0];
    mapType   = in[1];
    const std::size_t tsize =
        entries_for(indexType) * bytes_per_entry(mapType);
    if (tsize == 0) return 0;
    if (m_Content.size() != 2 + tsize) return 0;
    m_Table.assign(in + 2, in + 2 + tsize);
    return 1;
}

}  // namespace igtl
