// igtlServerSocket.h — drop-in facade for upstream's
// `igtl::ServerSocket`. Binds a listening TCP socket via
// oigtl::transport::tcp::listen() and yields accepted connections
// as ClientSocket instances.

#ifndef __igtlServerSocket_h
#define __igtlServerSocket_h

#include "igtlClientSocket.h"
#include "igtlMacro.h"
#include "igtlSocket.h"

#include <memory>

namespace igtl {

class IGTLCommon_EXPORT ServerSocket : public Socket {
 public:
    igtlTypeMacro(igtl::ServerSocket, igtl::Socket);
    igtlNewMacro(igtl::ServerSocket);

    /// Bind a listening socket on `port`. Returns 0 on success,
    /// -1 on failure. `port == 0` requests an ephemeral port;
    /// query with `GetServerPort()`.
    int CreateServer(int port);

    /// Block until a client connects, or `msec` milliseconds
    /// elapse (0 = no timeout, matches upstream). Returns a
    /// ClientSocket::Pointer on success, nullptr on timeout /
    /// error. Close the returned socket when done.
    ClientSocket::Pointer WaitForConnection(unsigned long msec = 0);

    /// Port the server is listening on (the actual port, not the
    /// one originally requested — useful after CreateServer(0)).
    int GetServerPort();

 protected:
    ServerSocket();
    ~ServerSocket() override;

    struct AcceptorPimpl;
    std::unique_ptr<AcceptorPimpl> m_AcceptorPimpl;

 private:
    ServerSocket(const ServerSocket&) = delete;
    void operator=(const ServerSocket&) = delete;
};

}  // namespace igtl

#endif  // __igtlServerSocket_h
