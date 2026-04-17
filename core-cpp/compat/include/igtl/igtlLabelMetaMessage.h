// Hand-written facade — supersedes the codegen stub.
// LBMETA — list of 116-byte label metadata records.
#ifndef __igtlLabelMetaMessage_h
#define __igtlLabelMetaMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlObject.h"
#include "igtlTypes.h"

#include <string>
#include <vector>

namespace igtl {

class IGTLCommon_EXPORT LabelMetaElement : public Object {
 public:
    igtlTypeMacro(igtl::LabelMetaElement, igtl::Object);
    igtlNewMacro(igtl::LabelMetaElement);

    int         SetName(const char* n);
    const char* GetName() { return m_Name.c_str(); }
    int         SetDeviceName(const char* d);
    const char* GetDeviceName() { return m_DeviceName.c_str(); }

    void      SetLabel(igtlUint8 l) { m_Label = l; }
    igtlUint8 GetLabel()            { return m_Label; }

    void SetRGBA(igtlUint8 rgba[4]);
    void SetRGBA(igtlUint8 r, igtlUint8 g, igtlUint8 b, igtlUint8 a);
    void GetRGBA(igtlUint8* rgba);
    void GetRGBA(igtlUint8& r, igtlUint8& g, igtlUint8& b, igtlUint8& a);

    void SetSize(igtlUint16 s[3]);
    void SetSize(igtlUint16 si, igtlUint16 sj, igtlUint16 sk);
    void GetSize(igtlUint16* s);

    int         SetOwner(const char* o);
    const char* GetOwner() { return m_Owner.c_str(); }

 protected:
    LabelMetaElement();
    ~LabelMetaElement() override = default;

    std::string m_Name;
    std::string m_DeviceName;
    igtlUint8   m_Label;
    igtlUint8   m_RGBA[4];
    igtlUint16  m_Size[3];
    std::string m_Owner;
};

class IGTLCommon_EXPORT LabelMetaMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::LabelMetaMessage, igtl::MessageBase);
    igtlNewMacro(igtl::LabelMetaMessage);

    int  AddLabelMetaElement(LabelMetaElement::Pointer& e);
    void ClearLabelMetaElement();
    int  GetNumberOfLabelMetaElement();
    void GetLabelMetaElement(int i, LabelMetaElement::Pointer& e);

 protected:
    LabelMetaMessage();
    ~LabelMetaMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    std::vector<LabelMetaElement::Pointer> m_LabelMetaList;
};

}  // namespace igtl

#endif  // __igtlLabelMetaMessage_h
