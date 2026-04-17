// Hand-written facade — supersedes the codegen stub.
// IMGMETA — list of 260-byte image metadata records.
#ifndef __igtlImageMetaMessage_h
#define __igtlImageMetaMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlObject.h"
#include "igtlTimeStamp.h"
#include "igtlTypes.h"

#include <string>
#include <vector>

namespace igtl {

class IGTLCommon_EXPORT ImageMetaElement : public Object {
 public:
    igtlTypeMacro(igtl::ImageMetaElement, igtl::Object);
    igtlNewMacro(igtl::ImageMetaElement);

    int         SetName(const char* n);
    const char* GetName() { return m_Name.c_str(); }
    int         SetDeviceName(const char* d);
    const char* GetDeviceName() { return m_DeviceName.c_str(); }
    int         SetModality(const char* m);
    const char* GetModality() { return m_Modality.c_str(); }
    int         SetPatientName(const char* p);
    const char* GetPatientName() { return m_PatientName.c_str(); }
    int         SetPatientID(const char* p);
    const char* GetPatientID() { return m_PatientID.c_str(); }

    void SetTimeStamp(TimeStamp::Pointer& t);
    void GetTimeStamp(TimeStamp::Pointer& t);

    void SetSize(igtlUint16 s[3]);
    void SetSize(igtlUint16 si, igtlUint16 sj, igtlUint16 sk);
    void GetSize(igtlUint16* s);
    void GetSize(igtlUint16& si, igtlUint16& sj, igtlUint16& sk);

    void      SetScalarType(igtlUint8 t) { m_ScalarType = t; }
    igtlUint8 GetScalarType() { return m_ScalarType; }

 protected:
    ImageMetaElement();
    ~ImageMetaElement() override = default;

    std::string m_Name;
    std::string m_DeviceName;
    std::string m_Modality;
    std::string m_PatientName;
    std::string m_PatientID;
    igtlUint64  m_TimeStamp;
    igtlUint16  m_Size[3];
    igtlUint8   m_ScalarType;
};

class IGTLCommon_EXPORT ImageMetaMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::ImageMetaMessage, igtl::MessageBase);
    igtlNewMacro(igtl::ImageMetaMessage);

    int  AddImageMetaElement(ImageMetaElement::Pointer& e);
    void ClearImageMetaElement();
    int  GetNumberOfImageMetaElement();
    void GetImageMetaElement(int index,
                             ImageMetaElement::Pointer& e);

 protected:
    ImageMetaMessage();
    ~ImageMetaMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    std::vector<ImageMetaElement::Pointer> m_ImageMetaList;
};

}  // namespace igtl

#endif  // __igtlImageMetaMessage_h
