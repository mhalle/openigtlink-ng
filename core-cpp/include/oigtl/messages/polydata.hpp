// GENERATED from spec/schemas/polydata.json — do not edit.
//
// Regenerate with: uv run oigtl-corpus codegen cpp
//
// POLYDATA message body codec.
#ifndef OIGTL_MESSAGES_POLYDATA_HPP
#define OIGTL_MESSAGES_POLYDATA_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oigtl::messages {

struct Polydata {
    static constexpr const char* kTypeId = "POLYDATA";

    struct Point {
        float x{};
        float y{};
        float z{};
    };
    struct AttributeHeader {
        std::uint8_t type{};
        std::uint8_t ncomponents{};
        std::uint32_t n{};
    };

    std::uint32_t npoints{};
    std::uint32_t nvertices{};
    std::uint32_t size_vertices{};
    std::uint32_t nlines{};
    std::uint32_t size_lines{};
    std::uint32_t npolygons{};
    std::uint32_t size_polygons{};
    std::uint32_t ntriangle_strips{};
    std::uint32_t size_triangle_strips{};
    std::uint32_t nattributes{};
    std::vector<Point> points;
    std::vector<std::uint8_t> vertices;
    std::vector<std::uint8_t> lines;
    std::vector<std::uint8_t> polygons;
    std::vector<std::uint8_t> triangle_strips;
    std::vector<AttributeHeader> attribute_headers;
    std::vector<std::uint8_t> attribute_data;

    // Pack body to a freshly-allocated big-endian byte vector.
    std::vector<std::uint8_t> pack() const;

    // Unpack a body. Throws ShortBufferError on truncation,
    // MalformedMessageError on shape errors (e.g. trailing bytes
    // not divisible by an element size).
    static Polydata unpack(const std::uint8_t* data, std::size_t length);
};

}  // namespace oigtl::messages

#endif  // OIGTL_MESSAGES_POLYDATA_HPP
