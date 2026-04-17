// Hand-written facade — supersedes the codegen stub.
// COLORT — color table / lookup.
// Body: 2-byte header + table bytes.
//   1 B index_type  (3=UINT8/256 entries, 5=UINT16/65536 entries)
//   1 B map_type    (3=UINT8, 5=UINT16, 19=RGB)
//   N*S bytes table
#ifndef __igtlColorTableMessage_h
#define __igtlColorTableMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlTypes.h"

#include <vector>

namespace igtl {

class IGTLCommon_EXPORT ColorTableMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::ColorTableMessage, igtl::MessageBase);
    igtlNewMacro(igtl::ColorTableMessage);

    enum {
        INDEX_UINT8  = 3,
        INDEX_UINT16 = 5,
        MAP_UINT8    = 3,
        MAP_UINT16   = 5,
        MAP_RGB      = 19,
    };

    void SetIndexType(int t)     { indexType = t; }
    void SetIndexTypeToUint8()   { indexType = INDEX_UINT8; }
    void SetIndexTypeToUint16()  { indexType = INDEX_UINT16; }
    int  GetIndexType()          { return indexType; }

    void SetMapType(int t)       { mapType = t; }
    void SetMapTypeToUint8()     { mapType = MAP_UINT8; }
    void SetMapTypeToUint16()    { mapType = MAP_UINT16; }
    int  GetMapType()            { return mapType; }

    int   GetColorTableSize();
    void* GetTablePointer();

    // Allocate the table buffer based on the current index/map types.
    // Caller fills bytes via GetTablePointer().
    void AllocateTable();

 protected:
    ColorTableMessage();
    ~ColorTableMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    int                       indexType;
    int                       mapType;
    std::vector<std::uint8_t> m_Table;
};

}  // namespace igtl

#endif  // __igtlColorTableMessage_h
