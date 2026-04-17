// igtlClientSocket.h — drop-in facade for upstream's
// `igtl::ClientSocket`. Connects to a server via
// oigtl::transport::tcp::connect() and hands the resulting
// Connection to the base Socket.

#ifndef __igtlClientSocket_h
#define __igtlClientSocket_h

#include "igtlMacro.h"
#include "igtlSocket.h"

namespace igtl {

class ServerSocket;

class IGTLCommon_EXPORT ClientSocket : public Socket {
 public:
    igtlTypeMacro(igtl::ClientSocket, igtl::Socket);
    igtlNewMacro(igtl::ClientSocket);

    /// Connect to `hostname:port`. Returns 0 on success, -1 on
    /// failure (DNS / connect error). `logErrorIfServerConnectionFailed`
    /// is accepted for API compat — logging is currently suppressed
    /// regardless, but the flag is preserved for potential future
    /// reintroduction of a diagnostics channel.
    int ConnectToServer(const char* hostname, int port,
                        bool logErrorIfServerConnectionFailed = true);

 protected:
    ClientSocket();
    ~ClientSocket() override;

    friend class ServerSocket;

 private:
    ClientSocket(const ClientSocket&) = delete;
    void operator=(const ClientSocket&) = delete;
};

}  // namespace igtl

#endif  // __igtlClientSocket_h
