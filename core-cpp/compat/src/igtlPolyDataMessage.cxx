// Hand-written POLYDATA facade.

#include "igtl/igtlPolyDataMessage.h"

#include <algorithm>
#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

namespace bo = oigtl::runtime::byte_order;

// ============ PolyDataPointArray ============
int PolyDataPointArray::AddPoint(igtlFloat32 p[3]) {
    Point q; q.xyz[0] = p[0]; q.xyz[1] = p[1]; q.xyz[2] = p[2];
    m_Data.push_back(q);
    return static_cast<int>(m_Data.size());
}
int PolyDataPointArray::AddPoint(igtlFloat32 x, igtlFloat32 y,
                                  igtlFloat32 z) {
    Point q; q.xyz[0] = x; q.xyz[1] = y; q.xyz[2] = z;
    m_Data.push_back(q);
    return static_cast<int>(m_Data.size());
}
int PolyDataPointArray::SetPoint(unsigned int id, igtlFloat32 p[3]) {
    if (id >= m_Data.size()) return 0;
    m_Data[id].xyz[0] = p[0];
    m_Data[id].xyz[1] = p[1];
    m_Data[id].xyz[2] = p[2];
    return 1;
}
int PolyDataPointArray::SetPoint(unsigned int id,
                                  igtlFloat32 x, igtlFloat32 y,
                                  igtlFloat32 z) {
    if (id >= m_Data.size()) return 0;
    m_Data[id].xyz[0] = x;
    m_Data[id].xyz[1] = y;
    m_Data[id].xyz[2] = z;
    return 1;
}
int PolyDataPointArray::GetPoint(unsigned int id, igtlFloat32* p) {
    if (id >= m_Data.size()) return 0;
    p[0] = m_Data[id].xyz[0];
    p[1] = m_Data[id].xyz[1];
    p[2] = m_Data[id].xyz[2];
    return 1;
}
int PolyDataPointArray::GetPoint(unsigned int id,
                                  igtlFloat32& x, igtlFloat32& y,
                                  igtlFloat32& z) {
    if (id >= m_Data.size()) return 0;
    x = m_Data[id].xyz[0];
    y = m_Data[id].xyz[1];
    z = m_Data[id].xyz[2];
    return 1;
}

// ============ PolyDataCellArray ============
void PolyDataCellArray::AddCell(int n, const igtlUint32* cell) {
    Cell c(cell, cell + n);
    m_Data.push_back(std::move(c));
}
void PolyDataCellArray::AddCell(const Cell& cell) {
    m_Data.push_back(cell);
}
igtlUint32 PolyDataCellArray::GetNumberOfCells() {
    return static_cast<igtlUint32>(m_Data.size());
}
igtlUint32 PolyDataCellArray::GetTotalSize() {
    igtlUint32 total = 0;
    for (auto& c : m_Data) {
        total += static_cast<igtlUint32>(c.size() + 1);
    }
    return total * sizeof(igtlUint32);
}
igtlUint32 PolyDataCellArray::GetCellSize(unsigned int id) {
    if (id >= m_Data.size()) return 0;
    return static_cast<igtlUint32>(m_Data[id].size());
}
int PolyDataCellArray::GetCell(unsigned int id, igtlUint32* out) {
    if (id >= m_Data.size()) return 0;
    const auto& c = m_Data[id];
    for (std::size_t i = 0; i < c.size(); ++i) out[i] = c[i];
    return 1;
}
int PolyDataCellArray::GetCell(unsigned int id, Cell& out) {
    if (id >= m_Data.size()) return 0;
    out = m_Data[id];
    return 1;
}

// ============ PolyDataAttribute ============
PolyDataAttribute::PolyDataAttribute()
    : m_Type(POINT_SCALAR), m_NComponents(1), m_N(0) {}

int PolyDataAttribute::SetType(int t, int nc) {
    m_Type        = static_cast<igtlUint8>(t);
    m_NComponents = static_cast<igtlUint32>(nc);
    return 1;
}
igtlUint32 PolyDataAttribute::SetSize(igtlUint32 size) {
    m_N = size;
    m_Data.resize(m_NComponents * m_N);
    return m_N;
}
void PolyDataAttribute::SetName(const char* name) {
    m_Name = name ? name : "";
}
int PolyDataAttribute::SetData(const igtlFloat32* data) {
    if (!data) return 0;
    const std::size_t n = m_NComponents * m_N;
    m_Data.assign(data, data + n);
    return 1;
}
int PolyDataAttribute::GetData(igtlFloat32* out) {
    if (!out) return 0;
    std::memcpy(out, m_Data.data(),
                m_Data.size() * sizeof(igtlFloat32));
    return 1;
}

// ============ helpers for the attribute name field ============
namespace {
// Each name is length-prefixed with a uint16, and the name+padding
// is aligned to the next uint16 boundary (so total consumed =
// 2 + name_len, rounded up to even).
std::size_t attr_name_padded_size(std::size_t name_len) {
    std::size_t total = 2 + name_len;
    if (total & 1) ++total;
    return total;
}

std::size_t attribute_wire_size(const PolyDataAttribute& a) {
    return 1 /*type*/ + 1 /*ncomp*/ + 4 /*n*/
         + attr_name_padded_size(a.m_Name.size())
         + 4 * a.m_NComponents * a.m_N;
}
}  // namespace

// ============ PolyDataMessage ============
PolyDataMessage::PolyDataMessage() {
    m_SendMessageType = "POLYDATA";
}

void PolyDataMessage::Clear() {
    m_Points.operator=(nullptr);
    m_Vertices.operator=(nullptr);
    m_Lines.operator=(nullptr);
    m_Polygons.operator=(nullptr);
    m_TriangleStrips.operator=(nullptr);
    m_Attributes.clear();
}

void PolyDataMessage::AddAttribute(PolyDataAttribute* att) {
    m_Attributes.emplace_back(att);
}

igtlUint64 PolyDataMessage::CalculateContentBufferSize() {
    std::size_t total = 40;   // fixed header
    if (m_Points.IsNotNull()) {
        total += 12 * m_Points->GetNumberOfPoints();
    }
    if (m_Vertices.IsNotNull())        total += m_Vertices->GetTotalSize();
    if (m_Lines.IsNotNull())           total += m_Lines->GetTotalSize();
    if (m_Polygons.IsNotNull())        total += m_Polygons->GetTotalSize();
    if (m_TriangleStrips.IsNotNull())  total += m_TriangleStrips->GetTotalSize();
    for (auto& a : m_Attributes) total += attribute_wire_size(*a);
    return total;
}

namespace {
void pack_cells(std::uint8_t*& out, PolyDataCellArray::Pointer& ca) {
    if (ca.IsNull()) return;
    const auto n = ca->GetNumberOfCells();
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint32_t sz = ca->GetCellSize(i);
        bo::write_be_u32(out, sz); out += 4;
        std::vector<igtlUint32> tmp(sz);
        ca->GetCell(i, tmp.data());
        for (auto v : tmp) {
            bo::write_be_u32(out, v); out += 4;
        }
    }
}
}  // namespace

int PolyDataMessage::PackContent() {
    const std::size_t need =
        static_cast<std::size_t>(CalculateContentBufferSize());
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();
    auto* hdr = out;

    const igtlUint32 npoints =
        m_Points.IsNotNull()
            ? static_cast<igtlUint32>(m_Points->GetNumberOfPoints())
            : 0;
    const igtlUint32 nvertices =
        m_Vertices.IsNotNull() ? m_Vertices->GetNumberOfCells() : 0;
    const igtlUint32 sz_vertices =
        m_Vertices.IsNotNull() ? m_Vertices->GetTotalSize() : 0;
    const igtlUint32 nlines =
        m_Lines.IsNotNull() ? m_Lines->GetNumberOfCells() : 0;
    const igtlUint32 sz_lines =
        m_Lines.IsNotNull() ? m_Lines->GetTotalSize() : 0;
    const igtlUint32 npolygons =
        m_Polygons.IsNotNull() ? m_Polygons->GetNumberOfCells() : 0;
    const igtlUint32 sz_polygons =
        m_Polygons.IsNotNull() ? m_Polygons->GetTotalSize() : 0;
    const igtlUint32 nstrips =
        m_TriangleStrips.IsNotNull()
            ? m_TriangleStrips->GetNumberOfCells() : 0;
    const igtlUint32 sz_strips =
        m_TriangleStrips.IsNotNull()
            ? m_TriangleStrips->GetTotalSize() : 0;
    const igtlUint32 nattrs =
        static_cast<igtlUint32>(m_Attributes.size());

    bo::write_be_u32(hdr + 0,  npoints);
    bo::write_be_u32(hdr + 4,  nvertices);
    bo::write_be_u32(hdr + 8,  sz_vertices);
    bo::write_be_u32(hdr + 12, nlines);
    bo::write_be_u32(hdr + 16, sz_lines);
    bo::write_be_u32(hdr + 20, npolygons);
    bo::write_be_u32(hdr + 24, sz_polygons);
    bo::write_be_u32(hdr + 28, nstrips);
    bo::write_be_u32(hdr + 32, sz_strips);
    bo::write_be_u32(hdr + 36, nattrs);

    auto* cur = out + 40;

    // Points.
    for (igtlUint32 i = 0; i < npoints; ++i) {
        igtlFloat32 p[3]; m_Points->GetPoint(i, p);
        bo::write_be_f32(cur + 0, p[0]);
        bo::write_be_f32(cur + 4, p[1]);
        bo::write_be_f32(cur + 8, p[2]);
        cur += 12;
    }
    pack_cells(cur, m_Vertices);
    pack_cells(cur, m_Lines);
    pack_cells(cur, m_Polygons);
    pack_cells(cur, m_TriangleStrips);

    // Attributes.
    for (auto& a : m_Attributes) {
        *cur++ = a->m_Type;
        *cur++ = static_cast<std::uint8_t>(a->m_NComponents);
        bo::write_be_u32(cur, a->m_N); cur += 4;
        const std::uint16_t nl =
            static_cast<std::uint16_t>(a->m_Name.size());
        bo::write_be_u16(cur, nl); cur += 2;
        if (nl > 0) std::memcpy(cur, a->m_Name.data(), nl);
        // pad to uint16 alignment (so (2+nl) is even)
        const std::size_t pad = (nl & 1) ? 1 : 0;
        cur += nl + pad;
        // data
        for (igtlUint32 i = 0; i < a->m_NComponents * a->m_N; ++i) {
            bo::write_be_f32(cur, a->m_Data[i]);
            cur += 4;
        }
    }
    return 1;
}

namespace {
bool unpack_cells(const std::uint8_t*& in, const std::uint8_t* end,
                  igtlUint32 ncells, igtlUint32 size_bytes,
                  PolyDataCellArray::Pointer& out) {
    if (in + size_bytes > end) return false;
    const std::uint8_t* const start = in;
    auto ca = PolyDataCellArray::New();
    for (igtlUint32 i = 0; i < ncells; ++i) {
        if (in + 4 > end) return false;
        const igtlUint32 sz = bo::read_be_u32(in); in += 4;
        if (in + sz * 4 > end) return false;
        PolyDataCellArray::Cell c(sz);
        for (igtlUint32 k = 0; k < sz; ++k) {
            c[k] = bo::read_be_u32(in); in += 4;
        }
        ca->AddCell(c);
    }
    out = ca;
    // in pointer is consistent with the ncells-walk above; size_bytes
    // is redundant but preserved for future forward-compat.
    (void)start; (void)size_bytes;
    return true;
}
}  // namespace

int PolyDataMessage::UnpackContent() {
    if (m_Content.size() < 40) return 0;
    Clear();
    const auto* in = m_Content.data();
    const auto* end = in + m_Content.size();
    const igtlUint32 npoints     = bo::read_be_u32(in + 0);
    const igtlUint32 nvertices   = bo::read_be_u32(in + 4);
    const igtlUint32 sz_vertices = bo::read_be_u32(in + 8);
    const igtlUint32 nlines      = bo::read_be_u32(in + 12);
    const igtlUint32 sz_lines    = bo::read_be_u32(in + 16);
    const igtlUint32 npolygons   = bo::read_be_u32(in + 20);
    const igtlUint32 sz_polys    = bo::read_be_u32(in + 24);
    const igtlUint32 nstrips     = bo::read_be_u32(in + 28);
    const igtlUint32 sz_strips   = bo::read_be_u32(in + 32);
    const igtlUint32 nattrs      = bo::read_be_u32(in + 36);

    const auto* cur = in + 40;

    if (npoints > 0) {
        if (cur + 12 * npoints > end) return 0;
        m_Points = PolyDataPointArray::New();
        for (igtlUint32 i = 0; i < npoints; ++i) {
            m_Points->AddPoint(
                bo::read_be_f32(cur + 0),
                bo::read_be_f32(cur + 4),
                bo::read_be_f32(cur + 8));
            cur += 12;
        }
    }
    if (!unpack_cells(cur, end, nvertices, sz_vertices, m_Vertices)) return 0;
    if (!unpack_cells(cur, end, nlines,    sz_lines,    m_Lines))    return 0;
    if (!unpack_cells(cur, end, npolygons, sz_polys,    m_Polygons)) return 0;
    if (!unpack_cells(cur, end, nstrips,   sz_strips,   m_TriangleStrips)) return 0;

    for (igtlUint32 i = 0; i < nattrs; ++i) {
        if (cur + 8 > end) return 0;
        auto a = PolyDataAttribute::New();
        a->m_Type        = *cur++;
        a->m_NComponents = *cur++;
        a->m_N           = bo::read_be_u32(cur); cur += 4;
        const std::uint16_t nl = bo::read_be_u16(cur); cur += 2;
        if (cur + nl > end) return 0;
        a->m_Name.assign(reinterpret_cast<const char*>(cur), nl);
        cur += nl;
        if (nl & 1) ++cur;  // pad
        const std::size_t dcount = a->m_NComponents * a->m_N;
        if (cur + 4 * dcount > end) return 0;
        a->m_Data.resize(dcount);
        for (std::size_t k = 0; k < dcount; ++k) {
            a->m_Data[k] = bo::read_be_f32(cur); cur += 4;
        }
        m_Attributes.emplace_back(a);
    }
    return 1;
}

// ============ RTSPolyDataMessage ============
RTSPolyDataMessage::RTSPolyDataMessage() : m_Status(0) {
    m_SendMessageType = "RTS_POLYDATA";
}
int RTSPolyDataMessage::PackContent() {
    if (m_Content.size() < 1) m_Content.assign(1, 0);
    m_Content[0] = m_Status;
    return 1;
}
int RTSPolyDataMessage::UnpackContent() {
    if (m_Content.size() < 1) return 0;
    m_Status = m_Content[0];
    return 1;
}

}  // namespace igtl
