// Hand-written facade — supersedes the codegen stub. If codegen
// runs, it skips this class (see cpp_compat.py skip list).
//
// STATUS — device operational status or request outcome.
// Wire layout (per spec/schemas/status.json):
//   2 B  uint16 code        — status code (1 = OK, 2 = unknown, ...)
//   8 B  int64  subcode     — device-specific subcode
//  20 B  char[20] error_name — null-padded ASCII (upstream quirk:
//                              uses strncpy — may be not null-
//                              terminated if input is ≥ 20 chars)
//   N B  trailing string    — ASCII, null-terminated
// Total: 30 + strlen(status_message) + 1 bytes.
#ifndef __igtlStatusMessage_h
#define __igtlStatusMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlTypes.h"

namespace igtl {

class IGTLCommon_EXPORT StatusMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::StatusMessage, igtl::MessageBase);
    igtlNewMacro(igtl::StatusMessage);

    // Upstream-matched status code constants. These values are
    // part of the wire protocol; any renumbering is a compat
    // break. Note STATUS_PANICK_MODE — upstream's spelling, kept
    // as-is for drop-in compat.
    enum {
        STATUS_INVALID             = 0,
        STATUS_OK                  = 1,
        STATUS_UNKNOWN_ERROR       = 2,
        STATUS_PANICK_MODE         = 3,
        STATUS_NOT_FOUND           = 4,
        STATUS_ACCESS_DENIED       = 5,
        STATUS_BUSY                = 6,
        STATUS_TIME_OUT            = 7,
        STATUS_OVERFLOW            = 8,
        STATUS_CHECKSUM_ERROR      = 9,
        STATUS_CONFIG_ERROR        = 10,
        STATUS_RESOURCE_ERROR      = 11,
        STATUS_UNKNOWN_INSTRUCTION = 12,
        STATUS_NOT_READY           = 13,
        STATUS_MANUAL_MODE         = 14,
        STATUS_DISABLED            = 15,
        STATUS_NOT_PRESENT         = 16,
        STATUS_UNKNOWN_VERSION     = 17,
        STATUS_HARDWARE_FAILURE    = 18,
        STATUS_SHUT_DOWN           = 19,
        STATUS_NUM_TYPES           = 20,
    };

    void SetCode(int code);
    int  GetCode();

    void      SetSubCode(igtlInt64 subcode);
    igtlInt64 GetSubCode();

    void        SetErrorName(const char* name);
    const char* GetErrorName();

    void        SetStatusString(const char* str);
    const char* GetStatusString();

 protected:
    StatusMessage();
    ~StatusMessage() override;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    igtlUint16  m_Code;
    igtlInt64   m_SubCode;
    char        m_ErrorName[20];    // zero-initialised in ctor
    std::string m_StatusMessageString;
};

}  // namespace igtl

#endif  // __igtlStatusMessage_h
