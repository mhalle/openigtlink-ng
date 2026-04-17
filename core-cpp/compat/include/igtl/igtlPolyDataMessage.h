// Hand-written facade — supersedes the codegen stub.
//
// POLYDATA — polygonal mesh (points + cells + per-vertex/cell
// attributes). Complex wire layout:
//   40 B  fixed header (10 × uint32 counts/sizes)
//         — npoints, nvertices, size_vertices, nlines, size_lines,
//           npolygons, size_polygons, ntriangle_strips,
//           size_triangle_strips, nattributes
//  12*P   points (float32[3] × P)
//         — vertices/lines/polygons/triangle_strips sections
//           each: concat of (u32 count, u32[count] indices) per cell
//  attrs  per attribute: u8 type + u8 ncomponents + u32 n +
//                         u16 name_len + name[aligned] +
//                         float32[ncomponents*n]
#ifndef __igtlPolyDataMessage_h
#define __igtlPolyDataMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlObject.h"
#include "igtlTypes.h"

#include <string>
#include <vector>

namespace igtl {

// ---------------- PolyDataPointArray ----------------
class IGTLCommon_EXPORT PolyDataPointArray : public Object {
 public:
    igtlTypeMacro(igtl::PolyDataPointArray, igtl::Object);
    igtlNewMacro(igtl::PolyDataPointArray);

    struct Point { igtlFloat32 xyz[3]; };

    void Clear() { m_Data.clear(); }
    void SetNumberOfPoints(int n) {
        m_Data.resize(static_cast<std::size_t>(n));
    }
    int  GetNumberOfPoints() {
        return static_cast<int>(m_Data.size());
    }
    int  AddPoint(igtlFloat32 p[3]);
    int  AddPoint(igtlFloat32 x, igtlFloat32 y, igtlFloat32 z);
    int  SetPoint(unsigned int id, igtlFloat32 p[3]);
    int  SetPoint(unsigned int id,
                  igtlFloat32 x, igtlFloat32 y, igtlFloat32 z);
    int  GetPoint(unsigned int id, igtlFloat32* p);
    int  GetPoint(unsigned int id,
                  igtlFloat32& x, igtlFloat32& y, igtlFloat32& z);

    std::vector<Point> m_Data;

 protected:
    PolyDataPointArray() = default;
    ~PolyDataPointArray() override = default;
};

// ---------------- PolyDataCellArray ----------------
class IGTLCommon_EXPORT PolyDataCellArray : public Object {
 public:
    igtlTypeMacro(igtl::PolyDataCellArray, igtl::Object);
    igtlNewMacro(igtl::PolyDataCellArray);

    // `Cell` is a list of vertex indices.
    using Cell = std::vector<igtlUint32>;

    void       Clear() { m_Data.clear(); }
    void       AddCell(int n, const igtlUint32* cell);
    void       AddCell(const Cell& cell);
    igtlUint32 GetNumberOfCells();
    // Packed-size in bytes (sum of (count+1) × sizeof(u32)).
    igtlUint32 GetTotalSize();
    igtlUint32 GetCellSize(unsigned int id);
    int        GetCell(unsigned int id, igtlUint32* cell);
    int        GetCell(unsigned int id, Cell& cell);

    std::vector<Cell> m_Data;

 protected:
    PolyDataCellArray() = default;
    ~PolyDataCellArray() override = default;
};

// ---------------- PolyDataAttribute ----------------
class IGTLCommon_EXPORT PolyDataAttribute : public Object {
 public:
    igtlTypeMacro(igtl::PolyDataAttribute, igtl::Object);
    igtlNewMacro(igtl::PolyDataAttribute);

    // Type byte encodes category (low nibble) + class (high nibble).
    // Upstream definitions: SCALAR=0x00, VECTOR=0x01, NORMAL=0x02,
    // TENSOR=0x03, RGBA=0x04, class POINT_DATA=0x00, CELL_DATA=0x10.
    enum {
        POINT_SCALAR = 0x00, POINT_VECTOR = 0x01,
        POINT_NORMAL = 0x02, POINT_TENSOR = 0x03,
        POINT_RGBA   = 0x04,
        CELL_SCALAR  = 0x10, CELL_VECTOR  = 0x11,
        CELL_NORMAL  = 0x12, CELL_TENSOR  = 0x13,
        CELL_RGBA    = 0x14,
    };

    int         SetType(int t, int n_components = 1);
    igtlUint8   GetType()               { return m_Type; }
    igtlUint32  GetNumberOfComponents() { return m_NComponents; }

    igtlUint32  SetSize(igtlUint32 size);  // n_entries
    igtlUint32  GetSize()                  { return m_N; }

    void        SetName(const char* name);
    const char* GetName() { return m_Name.c_str(); }

    int SetData(const igtlFloat32* data);
    int GetData(igtlFloat32* data);

    igtlUint8                m_Type;
    igtlUint32               m_NComponents;
    igtlUint32               m_N;
    std::string              m_Name;
    std::vector<igtlFloat32> m_Data;

 protected:
    PolyDataAttribute();
    ~PolyDataAttribute() override = default;
};

// ---------------- PolyDataMessage ----------------
class IGTLCommon_EXPORT PolyDataMessage : public MessageBase {
 public:
    using AttributeList = std::vector<PolyDataAttribute::Pointer>;

    igtlTypeMacro(igtl::PolyDataMessage, igtl::MessageBase);
    igtlNewMacro(igtl::PolyDataMessage);

    void Clear();

    void ClearAttributes() { m_Attributes.clear(); }
    void AddAttribute(PolyDataAttribute* att);

    PolyDataPointArray::Pointer m_Points;
    PolyDataCellArray::Pointer  m_Vertices;
    PolyDataCellArray::Pointer  m_Lines;
    PolyDataCellArray::Pointer  m_Polygons;
    PolyDataCellArray::Pointer  m_TriangleStrips;
    AttributeList               m_Attributes;

 protected:
    PolyDataMessage();
    ~PolyDataMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;
};

// ---------------- RTSPolyDataMessage ----------------
class IGTLCommon_EXPORT RTSPolyDataMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::RTSPolyDataMessage, igtl::MessageBase);
    igtlNewMacro(igtl::RTSPolyDataMessage);

    bool GetStatus() const { return m_Status != 0; }
    void SetStatus(bool s) { m_Status = s ? 1 : 0; }

 protected:
    RTSPolyDataMessage();
    ~RTSPolyDataMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override { return 1; }
    int PackContent()   override;
    int UnpackContent() override;

    igtlUint8 m_Status;
};

}  // namespace igtl

#endif  // __igtlPolyDataMessage_h
