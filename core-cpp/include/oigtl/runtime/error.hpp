// Typed exception hierarchy for the OpenIGTLink wire codec.
//
// All codec failures derive from oigtl::error::ProtocolError. Callers
// that need a non-throwing API should use the try_unpack() variants
// the generator emits (Phase 2+).
#ifndef OIGTL_RUNTIME_ERROR_HPP
#define OIGTL_RUNTIME_ERROR_HPP

#include <stdexcept>
#include <string>

namespace oigtl::error {

class ProtocolError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ShortBufferError : public ProtocolError {
public:
    using ProtocolError::ProtocolError;
};

class CrcMismatchError : public ProtocolError {
public:
    using ProtocolError::ProtocolError;
};

class UnknownMessageTypeError : public ProtocolError {
public:
    using ProtocolError::ProtocolError;
};

class MalformedMessageError : public ProtocolError {
public:
    using ProtocolError::ProtocolError;
};

}  // namespace oigtl::error

#endif  // OIGTL_RUNTIME_ERROR_HPP
