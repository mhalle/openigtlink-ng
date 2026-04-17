// igtlMessageBase.h — base class for every upstream-shaped
// message facade.
//
// Pointer-stability contract (same as upstream):
//   GetPackPointer()     — raw pointer into an owned byte buffer
//                          containing the 58-byte header + body.
//   GetPackBodyPointer() — raw pointer = GetPackPointer() + 58.
//   GetPackSize()        — header + body size.
//   These pointers are invalidated by any subsequent call to
//   Pack(), AllocatePack(), or SetMessageHeader() — callers
//   must not hold them across such calls.
//
// Version support: v1 (body = content only) and v2 (body =
// [12-byte extended header][content][metadata]). v3 shares v2's
// layout; the shim accepts `SetHeaderVersion(3)` and emits the
// same bytes.
//
// Subclass contract: override PackContent() to serialise the
// typed body into m_Body (a vector<uint8_t> populated by the
// base before the call); override UnpackContent() to deserialise
// from the same region after the base has filled it. Base handles
// the 58-byte header, the extended header, and metadata.
#ifndef __igtlMessageBase_h
#define __igtlMessageBase_h

#include "igtlMacro.h"
#include "igtlObject.h"
#include "igtlSmartPointer.h"
#include "igtlTimeStamp.h"
#include "igtlTypes.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Metadata value encoding — IANA MIBenum. Upstream places this at
// global scope (in igtlutil/igtl_types.h, a C header), so we do too
// for drop-in API compat. Consumer code writes `IANA_TYPE_US_ASCII`,
// not `igtl::IANA_TYPE_US_ASCII`.
#ifndef __IANA_ENCODING_TYPE_defined
#define __IANA_ENCODING_TYPE_defined
enum IANA_ENCODING_TYPE {
    IANA_TYPE_US_ASCII = 3,
    IANA_TYPE_UTF_8    = 106,
};
#endif

namespace igtl {

// Unpack() status bitmask (matches upstream).
enum UnpackStatus {
    UNPACK_UNDEF  = 0x0000,
    UNPACK_HEADER = 0x0001,
    UNPACK_BODY   = 0x0002,
};

class MessageHeader;
class TimeStamp;

class IGTLCommon_EXPORT MessageBase : public Object {
 public:
    igtlTypeMacro(igtl::MessageBase, igtl::Object);

    static Pointer New();

    // Upstream exposes the Unpack() status constants as class-scope
    // members, not just as the namespace-scope enum above. Some
    // upstream examples (Receiver/ReceiveServer.cxx) spell them as
    // `headerMsg->UNPACK_BODY`. Mirror the values as static
    // constants for source compatibility.
    static constexpr int UNPACK_UNDEF  = igtl::UNPACK_UNDEF;
    static constexpr int UNPACK_HEADER = igtl::UNPACK_HEADER;
    static constexpr int UNPACK_BODY   = igtl::UNPACK_BODY;

    // ---- identity / header fields ----
    void SetHeaderVersion(unsigned short v) { m_HeaderVersion = v; }
    unsigned short GetHeaderVersion() const { return m_HeaderVersion; }

    void SetDeviceName(const char* name);
    void SetDeviceName(const std::string& name);
    std::string GetDeviceName() const;

    void SetDeviceType(const std::string& type) { m_DeviceType = type; }
    // GetDeviceType returns the IGTL message type (e.g. "TRANSFORM").
    // Upstream's `GetDeviceType()` is misnamed — it is the type_id
    // field of the message, NOT the ITK-style object class name.
    // Every consumer relies on that misname — `strcmp(msg->
    // GetDeviceType(), "TRANSFORM")` is the universal dispatch.
    const char* GetDeviceType() const {
        return m_SendMessageType.c_str();
    }

    void SetMessageID(igtlUint32 id) { m_MessageID = id; }
    igtlUint32 GetMessageID() const  { return m_MessageID; }

    // IGTL 32.32 fixed-point seconds since epoch, as two 32-bit
    // halves. Upstream API.
    int SetTimeStamp(unsigned int sec, unsigned int frac);
    int GetTimeStamp(unsigned int* sec, unsigned int* frac);

    // TimeStamp-object overloads (upstream API — every example
    // program uses these). TimeStamp is forward-declared at
    // namespace scope above; the implementation lives in
    // igtlMessageBase.cxx and pulls in igtlTimeStamp.h.
    void SetTimeStamp(SmartPointer<TimeStamp>& ts);
    void GetTimeStamp(SmartPointer<TimeStamp>& ts);

    // ---- metadata (v2/v3) ----
    typedef std::pair<IANA_ENCODING_TYPE, std::string> MetaDataEntry;
    typedef std::map<std::string, MetaDataEntry> MetaDataMap;

    bool SetMetaDataElement(const std::string& key,
                            IANA_ENCODING_TYPE encoding,
                            std::string value);
    bool GetMetaDataElement(const std::string& key,
                            std::string& value) const;
    bool GetMetaDataElement(const std::string& key,
                            IANA_ENCODING_TYPE& encoding,
                            std::string& value) const;
    const MetaDataMap& GetMetaData() const { return m_MetaData; }

    // ---- pack / unpack pipeline ----
    virtual int Pack();
    int Unpack(int crccheck = 0);

    void* GetBufferPointer();
    void* GetPackPointer() { return GetBufferPointer(); }

    void* GetBufferBodyPointer();
    void* GetPackBodyPointer() { return GetBufferBodyPointer(); }

    igtl_uint64 GetBufferSize();
    igtl_uint64 GetPackSize() { return GetBufferSize(); }

    igtl_uint64 GetBufferBodySize();
    igtl_uint64 GetPackBodySize() { return GetBufferBodySize(); }

    // After a header has been deposited via SetMessageHeader (or
    // a previous InitPack + header recv), tells the caller how
    // many body bytes to read next.
    igtl_uint64 GetBodySizeToRead() const { return m_BodySizeToRead; }

    std::string GetMessageType() const { return m_SendMessageType; }
    const char* GetBodyType()          { return m_SendMessageType.c_str(); }

    // Allocate m_Body sized for the current BodySizeToRead (used on
    // the receive path, after the header is in).
    void AllocateBuffer();
    void AllocatePack() { AllocateBuffer(); }

    // Called before receiving a new header; allocates the 58-byte
    // header region and clears the unpacked flags.
    void InitBuffer();
    void InitPack() { InitBuffer(); }

    virtual int SetMessageHeader(const MessageHeader* mh);

 protected:
    MessageBase();
    ~MessageBase() override;

    // Subclass hooks — deserialise from / serialise to m_Body's
    // content region (which excludes ext-header and metadata).
    virtual int PackContent()   { return 0; }
    virtual int UnpackContent() { return 0; }

    // Subclass reports the packed content size so the base can
    // size the buffer before calling PackContent().
    virtual igtlUint64 CalculateContentBufferSize() { return 0; }

    // Fields the subclass identifies itself with. Set in ctor.
    std::string m_SendMessageType;   // e.g. "TRANSFORM"
    std::string m_DeviceType;

    // Byte region for the content (body minus ext-header and
    // metadata on v2+, or the whole body on v1). Subclass's
    // PackContent writes here; UnpackContent reads here.
    std::vector<std::uint8_t> m_Content;

    // ---- owned buffer — pointer-stable between Pack() calls ----
    std::vector<std::uint8_t> m_Wire;

    // Header fields (distinct from m_Wire so we can pack either
    // side of the line independently).
    unsigned short m_HeaderVersion = 2;
    std::string    m_DeviceName;
    igtlUint64     m_TimeStamp = 0;   // IGTL 32.32 fixed-point
    igtlUint32     m_MessageID = 0;

    MetaDataMap m_MetaData;

    // Set by SetMessageHeader from a received header; consumed
    // by AllocateBuffer + Unpack.
    igtl_uint64 m_BodySizeToRead = 0;

    // Tracks whether the body content has been unpacked by the
    // subclass. Reset by InitBuffer.
    bool m_IsBodyUnpacked = false;
    bool m_IsHeaderUnpacked = false;
};

}  // namespace igtl

#endif  // __igtlMessageBase_h
