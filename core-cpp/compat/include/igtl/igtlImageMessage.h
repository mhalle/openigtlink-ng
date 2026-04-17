// Hand-written facade — supersedes the codegen stub.
//
// IMAGE — 2D or 3D voxel volume with orientation + origin.
// Wire layout (per spec/schemas/image.json): 72-byte fixed header
// followed by raw pixel bytes.
//
// The 72-byte header:
//    2 B  header_version   (u16)
//    1 B  num_components   (u8, 1=scalar / 3=RGB or vector)
//    1 B  scalar_type      (u8, 2=int8..11=float64 — see TYPE_*)
//    1 B  endian           (u8, 1=big / 2=little)
//    1 B  coord            (u8, 1=RAS / 2=LPS)
//    6 B  size[3]          (u16, full-volume voxel extent)
//   48 B  matrix[12]       (f32, row-wise triples: scaled i/j/k
//                           axes + origin)
//    6 B  subvol_offset[3] (u16)
//    6 B  subvol_size[3]   (u16)
//
// Followed by num_components × prod(subvol_size) × sizeof(scalar_type)
// pixel bytes in the declared endianness.
#ifndef __igtlImageMessage_h
#define __igtlImageMessage_h

#include "igtlMacro.h"
#include "igtlMath.h"
#include "igtlMessageBase.h"
#include "igtlTypes.h"

#include <cstddef>

namespace igtl {

class IGTLCommon_EXPORT ImageMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::ImageMessage, igtl::MessageBase);
    igtlNewMacro(igtl::ImageMessage);

    enum { DTYPE_SCALAR = 1, DTYPE_VECTOR = 3 };

    enum {
        TYPE_INT8    = 2,
        TYPE_UINT8   = 3,
        TYPE_INT16   = 4,
        TYPE_UINT16  = 5,
        TYPE_INT32   = 6,
        TYPE_UINT32  = 7,
        TYPE_FLOAT32 = 10,
        TYPE_FLOAT64 = 11,
    };

    enum { ENDIAN_BIG = 1, ENDIAN_LITTLE = 2 };

    enum { COORDINATE_RAS = 1, COORDINATE_LPS = 2 };

    // ---- dimensions / sub-volume ----
    void SetDimensions(int s[3]);
    void SetDimensions(int i, int j, int k);
    void GetDimensions(int s[3]);
    void GetDimensions(int& i, int& j, int& k);

    int  SetSubVolume(int dim[3], int off[3]);
    int  SetSubVolume(int dimi, int dimj, int dimk,
                      int offi, int offj, int offk);
    void GetSubVolume(int dim[3], int off[3]);

    // ---- spatial params ----
    void SetSpacing(float s[3]);
    void SetSpacing(float si, float sj, float sk);
    void GetSpacing(float s[3]);

    void SetOrigin(float p[3]);
    void SetOrigin(float px, float py, float pz);
    void GetOrigin(float p[3]);

    void SetNormals(float o[3][3]);
    void GetNormals(float o[3][3]);

    void SetMatrix(Matrix4x4& mat);
    void GetMatrix(Matrix4x4& mat);

    // ---- components + scalar type ----
    void SetNumComponents(int n)  { numComponents = n; }
    int  GetNumComponents()       { return numComponents; }

    void SetScalarType(int t)     { scalarType = t; }
    int  GetScalarType()          { return scalarType; }
    void SetScalarTypeToInt8()    { scalarType = TYPE_INT8; }
    void SetScalarTypeToUint8()   { scalarType = TYPE_UINT8; }
    void SetScalarTypeToInt16()   { scalarType = TYPE_INT16; }
    void SetScalarTypeToUint16()  { scalarType = TYPE_UINT16; }
    void SetScalarTypeToInt32()   { scalarType = TYPE_INT32; }
    void SetScalarTypeToUint32()  { scalarType = TYPE_UINT32; }

    int  GetScalarSize();
    int  GetScalarSize(int type);

    void SetEndian(int e)         { endian = e; }
    int  GetEndian()              { return endian; }

    void SetCoordinateSystem(int c) { coordinate = c; }
    int  GetCoordinateSystem()    { return coordinate; }

    // ---- pixels ----
    // Size of the pixel payload in bytes — num_components *
    // prod(subDim) * scalarSize. Callable BEFORE AllocateScalars()
    // (the sizes are set by SetDimensions/SetSubVolume, not by
    // the allocation).
    int   GetImageSize();
    void  AllocateScalars();
    void* GetScalarPointer();

 protected:
    ImageMessage();
    ~ImageMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    int  dimensions[3];
    int  subDimensions[3];
    int  subOffset[3];
    float spacing[3];
    Matrix4x4 matrix;   // column 0..2 = i/j/k basis; col 3 = origin
    int  numComponents;
    int  scalarType;
    int  endian;
    int  coordinate;

    // Pixel buffer — separately owned from m_Content (upstream
    // exposes a pointer to this via GetScalarPointer()). PackContent
    // copies pixel bytes into m_Content after the 72-byte header;
    // UnpackContent splits m_Content back into header + pixels.
    std::vector<std::uint8_t> m_Pixels;
};

}  // namespace igtl

#endif  // __igtlImageMessage_h
