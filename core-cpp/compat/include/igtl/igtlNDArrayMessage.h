// Hand-written facade — supersedes the codegen stub.
//
// NDARRAY — n-dimensional array of primitive scalars.
// Body:   1 B scalar_type + 1 B dim + dim*2 B size[] + data.
// Total = 2 + 2*dim + product(size) * element_size(scalar_type).
//
// Upstream has a complex templated Array<T>/ArrayBase class
// hierarchy. Our shim provides the slice consumers of
// NDArrayMessage actually use: SetArray(type, ArrayBase*) /
// GetArray() / GetType(). ArrayBase carries the dimensions + a
// raw byte buffer; typed Array<T> is a thin wrapper.
#ifndef __igtlNDArrayMessage_h
#define __igtlNDArrayMessage_h

#include "igtlMacro.h"
#include "igtlMessageBase.h"
#include "igtlObject.h"
#include "igtlTypes.h"

#include <vector>

namespace igtl {

class IGTLCommon_EXPORT ArrayBase : public Object {
 public:
    igtlTypeMacro(igtl::ArrayBase, igtl::Object);

    typedef std::vector<igtlUint16> IndexType;

    enum {
        TYPE_INT8    = 2,
        TYPE_UINT8   = 3,
        TYPE_INT16   = 4,
        TYPE_UINT16  = 5,
        TYPE_INT32   = 6,
        TYPE_UINT32  = 7,
        TYPE_FLOAT32 = 10,
        TYPE_FLOAT64 = 11,
        TYPE_COMPLEX = 13,
    };

    int         SetSize(IndexType size);
    IndexType   GetSize()         { return m_Size; }
    int         GetDimension()    { return static_cast<int>(m_Size.size()); }
    igtlUint32  GetNumberOfElements();

    // Copy raw bytes into the array. Caller is responsible for
    // providing exactly GetRawArraySize() bytes.
    int        SetArray(const void* src);
    void*      GetRawArray()  { return m_ByteArray.data(); }
    igtlUint64 GetRawArraySize();

    virtual int GetElementSize() = 0;

 protected:
    ArrayBase();
    ~ArrayBase() override = default;

    IndexType                 m_Size;
    std::vector<std::uint8_t> m_ByteArray;
};

// Typed subclasses — one per NDARRAY scalar type. The only thing
// they carry is the per-element size; upstream's SetValue / GetValue
// are omitted (pack/unpack works purely via the byte array).
template <class T>
class Array : public ArrayBase {
 public:
    typedef Array                  Self;
    typedef ArrayBase              Superclass;
    typedef SmartPointer<Self>         Pointer;
    typedef SmartPointer<const Self>   ConstPointer;

    static Pointer New() {
        Pointer p = new Array;
        p->UnRegister();
        return p;
    }
    ::igtl::LightObject::Pointer CreateAnother() const override {
        return Array::New().GetPointer();
    }
    const char* GetNameOfClass() const override { return "Array<T>"; }

    int GetElementSize() override {
        return static_cast<int>(sizeof(T));
    }

 protected:
    Array() = default;
    ~Array() override = default;
};

class IGTLCommon_EXPORT NDArrayMessage : public MessageBase {
 public:
    igtlTypeMacro(igtl::NDArrayMessage, igtl::MessageBase);
    igtlNewMacro(igtl::NDArrayMessage);

    // Upstream mirrors these on NDArrayMessage itself.
    enum {
        TYPE_INT8    = ArrayBase::TYPE_INT8,
        TYPE_UINT8   = ArrayBase::TYPE_UINT8,
        TYPE_INT16   = ArrayBase::TYPE_INT16,
        TYPE_UINT16  = ArrayBase::TYPE_UINT16,
        TYPE_INT32   = ArrayBase::TYPE_INT32,
        TYPE_UINT32  = ArrayBase::TYPE_UINT32,
        TYPE_FLOAT32 = ArrayBase::TYPE_FLOAT32,
        TYPE_FLOAT64 = ArrayBase::TYPE_FLOAT64,
        TYPE_COMPLEX = ArrayBase::TYPE_COMPLEX,
    };

    int        SetArray(int type, ArrayBase* a);
    ArrayBase* GetArray() { return m_Array.GetPointer(); }
    int        GetType()  { return m_Type; }

 protected:
    NDArrayMessage();
    ~NDArrayMessage() override = default;

    igtlUint64 CalculateContentBufferSize() override;
    int PackContent()   override;
    int UnpackContent() override;

    ArrayBase::Pointer m_Array;
    int                m_Type;
};

}  // namespace igtl

#endif  // __igtlNDArrayMessage_h
