// GENERATED from spec/schemas/polydata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp

#include "oigtl/messages/polydata.hpp"

#include <cstring>
#include <sstream>

#include "oigtl/runtime/ascii.hpp"
#include "oigtl/runtime/byte_order.hpp"
#include "oigtl/runtime/error.hpp"

namespace oigtl::messages {

std::vector<std::uint8_t> Polydata::pack() const {
    const std::size_t body_size = (4) + (4) + (4) + (4) + (4) + (4) + (4) + (4) + (4) + (4) + (points.size() * 12) + (vertices.size() * 1) + (lines.size() * 1) + (polygons.size() * 1) + (triangle_strips.size() * 1) + (attribute_headers.size() * 6) + (attribute_data.size() * 1);
    std::vector<std::uint8_t> out(body_size);
    std::size_t off = 0;
    // npoints
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, npoints);
    off += 4;
    // nvertices
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, nvertices);
    off += 4;
    // size_vertices
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, size_vertices);
    off += 4;
    // nlines
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, nlines);
    off += 4;
    // size_lines
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, size_lines);
    off += 4;
    // npolygons
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, npolygons);
    off += 4;
    // size_polygons
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, size_polygons);
    off += 4;
    // ntriangle_strips
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, ntriangle_strips);
    off += 4;
    // size_triangle_strips
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, size_triangle_strips);
    off += 4;
    // nattributes
    oigtl::runtime::byte_order::write_be_u32(out.data() + off, nattributes);
    off += 4;
    // points
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& elem = points[i];
        oigtl::runtime::byte_order::write_be_f32(out.data() + off, elem.x);
        off += 4;
        oigtl::runtime::byte_order::write_be_f32(out.data() + off, elem.y);
        off += 4;
        oigtl::runtime::byte_order::write_be_f32(out.data() + off, elem.z);
        off += 4;
    }
    // (off advanced inside loop)
    // vertices
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, vertices[i]);
    }
    off += vertices.size() * 1;
    // lines
    for (std::size_t i = 0; i < lines.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, lines[i]);
    }
    off += lines.size() * 1;
    // polygons
    for (std::size_t i = 0; i < polygons.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, polygons[i]);
    }
    off += polygons.size() * 1;
    // triangle_strips
    for (std::size_t i = 0; i < triangle_strips.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, triangle_strips[i]);
    }
    off += triangle_strips.size() * 1;
    // attribute_headers
    for (std::size_t i = 0; i < attribute_headers.size(); ++i) {
        const auto& elem = attribute_headers[i];
        oigtl::runtime::byte_order::write_be_u8(out.data() + off, elem.type);
        off += 1;
        oigtl::runtime::byte_order::write_be_u8(out.data() + off, elem.ncomponents);
        off += 1;
        oigtl::runtime::byte_order::write_be_u32(out.data() + off, elem.n);
        off += 4;
    }
    // (off advanced inside loop)
    // attribute_data
    for (std::size_t i = 0; i < attribute_data.size(); ++i) {
        oigtl::runtime::byte_order::write_be_u8(out.data() + off + i * 1, attribute_data[i]);
    }
    off += attribute_data.size() * 1;
    (void)off;  // suppress unused-variable warning when body is empty
    return out;
}

Polydata Polydata::unpack(const std::uint8_t* data, std::size_t length) {
    Polydata out;
    std::size_t off = 0;
    // npoints
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("npoints: short buffer"); }
    out.npoints = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // nvertices
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("nvertices: short buffer"); }
    out.nvertices = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // size_vertices
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("size_vertices: short buffer"); }
    out.size_vertices = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // nlines
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("nlines: short buffer"); }
    out.nlines = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // size_lines
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("size_lines: short buffer"); }
    out.size_lines = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // npolygons
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("npolygons: short buffer"); }
    out.npolygons = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // size_polygons
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("size_polygons: short buffer"); }
    out.size_polygons = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // ntriangle_strips
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("ntriangle_strips: short buffer"); }
    out.ntriangle_strips = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // size_triangle_strips
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("size_triangle_strips: short buffer"); }
    out.size_triangle_strips = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // nattributes
    if (off + (4) > length) { throw oigtl::error::ShortBufferError("nattributes: short buffer"); }
    out.nattributes = oigtl::runtime::byte_order::read_be_u32(data + off);
    off += 4;
    // points
    {
        std::size_t count = static_cast<std::size_t>(out.npoints);
        if (off + (count * 12) > length) { throw oigtl::error::ShortBufferError("points: short buffer"); }
        out.points.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto& elem = out.points[i];
            elem.x = oigtl::runtime::byte_order::read_be_f32(data + off);
            off += 4;
            elem.y = oigtl::runtime::byte_order::read_be_f32(data + off);
            off += 4;
            elem.z = oigtl::runtime::byte_order::read_be_f32(data + off);
            off += 4;
        }
    }
    // vertices
    {
        std::size_t count = static_cast<std::size_t>(out.size_vertices);
        if (off + (count * 1) > length) { throw oigtl::error::ShortBufferError("vertices: short buffer"); }
        out.vertices.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.vertices[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += count * 1;
    }
    // lines
    {
        std::size_t count = static_cast<std::size_t>(out.size_lines);
        if (off + (count * 1) > length) { throw oigtl::error::ShortBufferError("lines: short buffer"); }
        out.lines.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.lines[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += count * 1;
    }
    // polygons
    {
        std::size_t count = static_cast<std::size_t>(out.size_polygons);
        if (off + (count * 1) > length) { throw oigtl::error::ShortBufferError("polygons: short buffer"); }
        out.polygons.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.polygons[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += count * 1;
    }
    // triangle_strips
    {
        std::size_t count = static_cast<std::size_t>(out.size_triangle_strips);
        if (off + (count * 1) > length) { throw oigtl::error::ShortBufferError("triangle_strips: short buffer"); }
        out.triangle_strips.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.triangle_strips[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += count * 1;
    }
    // attribute_headers
    {
        std::size_t count = static_cast<std::size_t>(out.nattributes);
        if (off + (count * 6) > length) { throw oigtl::error::ShortBufferError("attribute_headers: short buffer"); }
        out.attribute_headers.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            auto& elem = out.attribute_headers[i];
            elem.type = oigtl::runtime::byte_order::read_be_u8(data + off);
            off += 1;
            elem.ncomponents = oigtl::runtime::byte_order::read_be_u8(data + off);
            off += 1;
            elem.n = oigtl::runtime::byte_order::read_be_u32(data + off);
            off += 4;
        }
    }
    // attribute_data
    {
        std::size_t bytes = length - off;
        if (bytes % 1 != 0) { throw oigtl::error::MalformedMessageError("attribute_data: trailing bytes not divisible by element size"); }
        std::size_t count = bytes / 1;
        out.attribute_data.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            out.attribute_data[i] = oigtl::runtime::byte_order::read_be_u8(data + off + i * 1);
        }
        off += bytes;
    }
    (void)off;
    (void)data;
    (void)length;
    return out;
}

}  // namespace oigtl::messages
