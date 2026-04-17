// igtlSocket.h — upstream-compatible Socket facade backed by
// oigtl::transport::Connection.
//
// Upstream's `igtl::Socket` is a thin wrapper over a POSIX socket
// descriptor with two primary ops: `Send(data, len)` and
// `Receive(data, len, timeout, readFully)`. Every example program
// uses the pattern
//
//     socket->Receive(headerMsg->GetPackPointer(),     PackSize,     t);
//     headerMsg->Unpack();
//     // ... allocate body ...
//     socket->Receive(bodyMsg->GetPackBodyPointer(),   BodySize,     t);
//
// That is: arbitrary-sized reads, though in practice always exactly
// matching IGTL framing boundaries. The shim implements this by
// pulling one framed `Incoming` from the underlying Connection and
// then servicing byte-level Receive()s from that reassembled buffer,
// pulling a new message when the buffer drains.
//
// That keeps the transport's contract purely framed — Noise, when
// it lands in Phase 3, wraps `Connection::receive_sync()` and this
// shim needs no change.
//
// The asio / transport headers are NOT included here — they leak
// into everything that consumes the shim. Instead, `Socket` holds a
// `pimpl` whose real definition lives in igtlSocket.cxx.

#ifndef __igtlSocket_h
#define __igtlSocket_h

#include "igtlMacro.h"
#include "igtlObject.h"
#include "igtlObjectFactory.h"
#include "igtlTypes.h"

#include <memory>
#include <string>

namespace igtl {

class IGTLCommon_EXPORT Socket : public Object {
 public:
    igtlTypeMacro(igtl::Socket, igtl::Object);
    igtlNewMacro(igtl::Socket);

    // ---- connection state ----

    /// True once a Connection has been attached (by ClientSocket
    /// ConnectToServer, or ServerSocket WaitForConnection).
    bool GetConnected();

    /// Tear down the connection. Idempotent.
    void CloseSocket();

    // ---- data path ----

    /// Send `length` bytes of `data`. Returns 1 on success, 0 on
    /// error (peer closed, write failure, no connection). Matches
    /// upstream's return contract.
    int Send(const void* data, igtlUint64 length);

    /// Read `length` bytes into `data`. If `readFully` is set (the
    /// default, which every upstream example uses), blocks until
    /// all `length` bytes have been read, even across multiple
    /// underlying framed messages. On timeout, sets `timeout=true`
    /// and returns 0. On peer close / error, returns 0 and
    /// transitions to disconnected.
    igtlUint64 Receive(void* data,
                       igtlUint64 length,
                       bool& timeout,
                       int readFully = 1);

    /// Discard `length` bytes from the incoming stream. Uses the
    /// same reassembled-buffer state machine as Receive(). Returns
    /// 1 on success, 0 on error/timeout.
    int Skip(igtlUint64 length, int skipFully = 1);

    // ---- tunables ----

    /// Set both send and receive timeout, in milliseconds. 0 means
    /// block indefinitely.
    int SetTimeout(int timeout);

    /// Set receive timeout in milliseconds. 0 = block indefinitely.
    int SetReceiveTimeout(int timeout);

    /// Set send timeout in milliseconds. 0 = block indefinitely.
    int SetSendTimeout(int timeout);

    /// Pseudo non-blocking mode — sets the relevant timeout to a
    /// minimum value (1ms) when `sw=1`, or clears it otherwise.
    /// Implemented as a thin wrapper over the timeout setters,
    /// matching upstream's behaviour-but-not-semantics.
    int SetReceiveBlocking(int sw);
    int SetSendBlocking(int sw);

    // ---- introspection ----

    /// Populate `address` and `port` with the remote peer's
    /// endpoint. Returns 1 on success, 0 if unconnected.
    int GetSocketAddressAndPort(std::string& address, int& port);

    // ---- shim extension — not in upstream API ----

    /// Attach a pre-built transport::Connection. Used internally
    /// by ClientSocket::ConnectToServer and ServerSocket::
    /// WaitForConnection. Argument is an opaque owner; the real
    /// type is hidden to keep asio out of this header.
    void AttachConnection(void* connection_owner);

 protected:
    Socket();
    ~Socket() override;

    struct Pimpl;
    std::unique_ptr<Pimpl> m_Pimpl;

 private:
    Socket(const Socket&) = delete;
    void operator=(const Socket&) = delete;
};

}  // namespace igtl

#endif  // __igtlSocket_h
