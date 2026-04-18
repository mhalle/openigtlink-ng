// Hand-written ImageMessage facade. Wire layout matches upstream
// byte-for-byte: 72-byte fixed header + pixel bytes.
//
// Notable quirk: upstream's 48-byte matrix field is NOT the same
// shape as TRANSFORM's. For IMAGE:
//   wire[0..2]  = norm_i * spacing[0]   (= column 0 of matrix
//                                         times x-spacing)
//   wire[3..5]  = norm_j * spacing[1]
//   wire[6..8]  = norm_k * spacing[2]
//   wire[9..11] = origin                 (= column 3)
// i.e. row-major over three 3-element triples (i-axis*sx,
// j-axis*sy, k-axis*sz), then the 3-element origin.
// Upstream's impl: igtl_image_set_matrix in igtlutil/igtl_image.c.

#include "igtl/igtlImageMessage.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "oigtl/runtime/byte_order.hpp"

namespace igtl {

namespace {
constexpr std::size_t kHeaderSize = 72;

int scalar_size_for(int type) {
    switch (type) {
        case ImageMessage::TYPE_INT8:    return 1;
        case ImageMessage::TYPE_UINT8:   return 1;
        case ImageMessage::TYPE_INT16:   return 2;
        case ImageMessage::TYPE_UINT16:  return 2;
        case ImageMessage::TYPE_INT32:   return 4;
        case ImageMessage::TYPE_UINT32:  return 4;
        case ImageMessage::TYPE_FLOAT32: return 4;
        case ImageMessage::TYPE_FLOAT64: return 8;
        default:                          return 0;
    }
}
}  // namespace

ImageMessage::ImageMessage()
    : numComponents(1),
      scalarType(TYPE_UINT8),
      endian(ENDIAN_BIG),
      coordinate(COORDINATE_RAS) {
    m_SendMessageType = "IMAGE";
    dimensions[0] = dimensions[1] = dimensions[2] = 0;
    subDimensions[0] = subDimensions[1] = subDimensions[2] = 0;
    subOffset[0] = subOffset[1] = subOffset[2] = 0;
    spacing[0] = spacing[1] = spacing[2] = 1.0f;
    IdentityMatrix(matrix);
}

// ---- dimensions ----
void ImageMessage::SetDimensions(int s[3]) {
    SetDimensions(s[0], s[1], s[2]);
}
void ImageMessage::SetDimensions(int i, int j, int k) {
    dimensions[0] = i; dimensions[1] = j; dimensions[2] = k;
    // Matching upstream: reset sub-volume to the full volume
    // whenever dimensions change.
    subDimensions[0] = i; subDimensions[1] = j; subDimensions[2] = k;
    subOffset[0] = subOffset[1] = subOffset[2] = 0;
}
void ImageMessage::GetDimensions(int s[3]) {
    s[0] = dimensions[0]; s[1] = dimensions[1]; s[2] = dimensions[2];
}
void ImageMessage::GetDimensions(int& i, int& j, int& k) {
    i = dimensions[0]; j = dimensions[1]; k = dimensions[2];
}

int ImageMessage::SetSubVolume(int dim[3], int off[3]) {
    return SetSubVolume(dim[0], dim[1], dim[2],
                        off[0], off[1], off[2]);
}
int ImageMessage::SetSubVolume(int dimi, int dimj, int dimk,
                               int offi, int offj, int offk) {
    if (offi + dimi > dimensions[0] ||
        offj + dimj > dimensions[1] ||
        offk + dimk > dimensions[2]) return 0;
    subDimensions[0] = dimi; subDimensions[1] = dimj;
    subDimensions[2] = dimk;
    subOffset[0] = offi; subOffset[1] = offj;
    subOffset[2] = offk;
    return 1;
}
void ImageMessage::GetSubVolume(int dim[3], int off[3]) {
    dim[0] = subDimensions[0]; dim[1] = subDimensions[1];
    dim[2] = subDimensions[2];
    off[0] = subOffset[0]; off[1] = subOffset[1];
    off[2] = subOffset[2];
}

// ---- spacing / origin / normals / matrix ----
void ImageMessage::SetSpacing(float s[3]) {
    SetSpacing(s[0], s[1], s[2]);
}
void ImageMessage::SetSpacing(float si, float sj, float sk) {
    spacing[0] = si; spacing[1] = sj; spacing[2] = sk;
}
void ImageMessage::GetSpacing(float s[3]) {
    s[0] = spacing[0]; s[1] = spacing[1]; s[2] = spacing[2];
}

void ImageMessage::SetOrigin(float p[3]) {
    matrix[0][3] = p[0]; matrix[1][3] = p[1]; matrix[2][3] = p[2];
}
void ImageMessage::SetOrigin(float px, float py, float pz) {
    matrix[0][3] = px; matrix[1][3] = py; matrix[2][3] = pz;
}
void ImageMessage::GetOrigin(float p[3]) {
    p[0] = matrix[0][3]; p[1] = matrix[1][3]; p[2] = matrix[2][3];
}

void ImageMessage::SetNormals(float o[3][3]) {
    // Upstream: o[0][] = i-axis unit vector, o[1][] = j-axis, o[2][] = k
    for (int i = 0; i < 3; ++i) {
        matrix[i][0] = o[0][i];   // column 0 = i-axis
        matrix[i][1] = o[1][i];
        matrix[i][2] = o[2][i];
    }
}
void ImageMessage::GetNormals(float o[3][3]) {
    for (int i = 0; i < 3; ++i) {
        o[0][i] = matrix[i][0];
        o[1][i] = matrix[i][1];
        o[2][i] = matrix[i][2];
    }
}

void ImageMessage::SetMatrix(Matrix4x4& m) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            matrix[i][j] = m[i][j];
}
void ImageMessage::GetMatrix(Matrix4x4& m) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            m[i][j] = matrix[i][j];
}

int ImageMessage::GetScalarSize() { return scalar_size_for(scalarType); }
int ImageMessage::GetScalarSize(int t) { return scalar_size_for(t); }

int ImageMessage::GetImageSize() {
    return numComponents *
           subDimensions[0] * subDimensions[1] * subDimensions[2] *
           GetScalarSize();
}

void ImageMessage::AllocateScalars() {
    const std::size_t size = static_cast<std::size_t>(GetImageSize());
    m_Pixels.assign(size, 0);
}
void* ImageMessage::GetScalarPointer() { return m_Pixels.data(); }

// ---- wire packing ----
igtlUint64 ImageMessage::CalculateContentBufferSize() {
    return kHeaderSize +
           static_cast<igtlUint64>(GetImageSize());
}

int ImageMessage::PackContent() {
    namespace bo = oigtl::runtime::byte_order;
    const std::size_t img = static_cast<std::size_t>(GetImageSize());
    const std::size_t need = kHeaderSize + img;
    if (m_Content.size() < need) m_Content.assign(need, 0);
    auto* out = m_Content.data();

    // Fixed 72-byte header.
    bo::write_be_u16(out + 0, 1);                     // header_version
    out[2] = static_cast<std::uint8_t>(numComponents);
    out[3] = static_cast<std::uint8_t>(scalarType);
    out[4] = static_cast<std::uint8_t>(endian);
    out[5] = static_cast<std::uint8_t>(coordinate);
    bo::write_be_u16(out + 6,  static_cast<std::uint16_t>(dimensions[0]));
    bo::write_be_u16(out + 8,  static_cast<std::uint16_t>(dimensions[1]));
    bo::write_be_u16(out + 10, static_cast<std::uint16_t>(dimensions[2]));

    // 48-byte matrix: [norm_i*sx | norm_j*sy | norm_k*sz | origin]
    // where norm_i = matrix[:, 0], norm_j = matrix[:, 1], etc.
    for (int i = 0; i < 3; ++i) {
        bo::write_be_f32(out + 12 + i * 4,
                         matrix[i][0] * spacing[0]);
    }
    for (int i = 0; i < 3; ++i) {
        bo::write_be_f32(out + 24 + i * 4,
                         matrix[i][1] * spacing[1]);
    }
    for (int i = 0; i < 3; ++i) {
        bo::write_be_f32(out + 36 + i * 4,
                         matrix[i][2] * spacing[2]);
    }
    for (int i = 0; i < 3; ++i) {
        bo::write_be_f32(out + 48 + i * 4, matrix[i][3]);
    }

    bo::write_be_u16(out + 60, static_cast<std::uint16_t>(subOffset[0]));
    bo::write_be_u16(out + 62, static_cast<std::uint16_t>(subOffset[1]));
    bo::write_be_u16(out + 64, static_cast<std::uint16_t>(subOffset[2]));
    bo::write_be_u16(out + 66, static_cast<std::uint16_t>(subDimensions[0]));
    bo::write_be_u16(out + 68, static_cast<std::uint16_t>(subDimensions[1]));
    bo::write_be_u16(out + 70, static_cast<std::uint16_t>(subDimensions[2]));

    // Pixels — byte-for-byte from m_Pixels. Endianness is the
    // sender's declared `endian` field; we don't swap.
    if (img > 0 && !m_Pixels.empty()) {
        std::memcpy(out + kHeaderSize, m_Pixels.data(),
                    std::min(img, m_Pixels.size()));
    }
    return 1;
}

int ImageMessage::UnpackContent() {
    namespace bo = oigtl::runtime::byte_order;
    if (m_Content.size() < kHeaderSize) return 0;
    const auto* in = m_Content.data();

    // version at offset 0 ignored (we carry a single version).
    numComponents = in[2];
    scalarType    = in[3];
    endian        = in[4];
    coordinate    = in[5];

    dimensions[0] = bo::read_be_u16(in + 6);
    dimensions[1] = bo::read_be_u16(in + 8);
    dimensions[2] = bo::read_be_u16(in + 10);

    // 12 matrix floats: (i_axis*sx) | (j_axis*sy) | (k_axis*sz) | origin
    float vi[3], vj[3], vk[3], origin[3];
    for (int i = 0; i < 3; ++i) vi[i] = bo::read_be_f32(in + 12 + i * 4);
    for (int i = 0; i < 3; ++i) vj[i] = bo::read_be_f32(in + 24 + i * 4);
    for (int i = 0; i < 3; ++i) vk[i] = bo::read_be_f32(in + 36 + i * 4);
    for (int i = 0; i < 3; ++i) origin[i] = bo::read_be_f32(in + 48 + i * 4);

    // Extract spacing as magnitudes of v{i,j,k}; normals = v/spacing.
    auto norm = [](float v[3]) {
        float s = 0; for (int i = 0; i < 3; ++i) s += v[i] * v[i];
        return std::sqrt(s);
    };
    spacing[0] = norm(vi);
    spacing[1] = norm(vj);
    spacing[2] = norm(vk);

    for (int i = 0; i < 3; ++i) {
        matrix[i][0] = spacing[0] > 0.0f ? vi[i] / spacing[0] : 0.0f;
        matrix[i][1] = spacing[1] > 0.0f ? vj[i] / spacing[1] : 0.0f;
        matrix[i][2] = spacing[2] > 0.0f ? vk[i] / spacing[2] : 0.0f;
        matrix[i][3] = origin[i];
    }
    matrix[3][0] = matrix[3][1] = matrix[3][2] = 0.0f;
    matrix[3][3] = 1.0f;

    subOffset[0] = bo::read_be_u16(in + 60);
    subOffset[1] = bo::read_be_u16(in + 62);
    subOffset[2] = bo::read_be_u16(in + 64);
    subDimensions[0] = bo::read_be_u16(in + 66);
    subDimensions[1] = bo::read_be_u16(in + 68);
    subDimensions[2] = bo::read_be_u16(in + 70);

    // Validate pixel payload size exactly.
    const std::size_t expected = static_cast<std::size_t>(GetImageSize());
    if (m_Content.size() - kHeaderSize < expected) return 0;
    m_Pixels.assign(in + kHeaderSize,
                    in + kHeaderSize + expected);
    return 1;
}

}  // namespace igtl
