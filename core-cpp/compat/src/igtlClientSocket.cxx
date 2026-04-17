// igtlClientSocket.cxx — ConnectToServer delegates to
// oigtl::transport::tcp::connect() and hands the resulting
// Connection to the Socket base via AttachConnection().

#include "igtl/igtlClientSocket.h"

#include <memory>
#include <string>

#include "oigtl/transport/connection.hpp"
#include "oigtl/transport/tcp.hpp"

namespace igtl {

ClientSocket::ClientSocket()  = default;
ClientSocket::~ClientSocket() = default;

int ClientSocket::ConnectToServer(const char* hostname, int port,
                                  bool /*logErrorIfFailed*/) {
    try {
        auto conn = oigtl::transport::tcp::connect(
            std::string(hostname),
            static_cast<std::uint16_t>(port)).get();
        // AttachConnection takes an opaque void* to keep asio out
        // of the public header; it's actually a pointer to a
        // unique_ptr<Connection>.
        AttachConnection(static_cast<void*>(&conn));
        return 0;
    } catch (...) {
        return -1;
    }
}

}  // namespace igtl
