// igtlServerSocket.cxx — binds via oigtl::transport::tcp::listen()
// and yields accepted Connections as ClientSocket instances.
//
// Timeout on WaitForConnection: the ergonomic thing is
// `Future<...>::wait_for(msec)`, which returns true on resolution
// and false on timeout. On timeout we cancel the pending accept
// so the next call gets a fresh one. (Transport's cancel() is
// cooperative — the asio backend honors it.)

#include "igtl/igtlServerSocket.h"

#include <chrono>
#include <memory>

#include "oigtl/transport/connection.hpp"
#include "oigtl/transport/tcp.hpp"

namespace igtl {

struct ServerSocket::AcceptorPimpl {
    std::unique_ptr<oigtl::transport::Acceptor> acceptor;
    int server_port = 0;
};

ServerSocket::ServerSocket()  : m_AcceptorPimpl(new AcceptorPimpl) {}
ServerSocket::~ServerSocket() {
    if (m_AcceptorPimpl && m_AcceptorPimpl->acceptor) {
        try {
            m_AcceptorPimpl->acceptor->close().get();
        } catch (...) {}
    }
}

int ServerSocket::CreateServer(int port) {
    try {
        auto acc = oigtl::transport::tcp::listen(
            static_cast<std::uint16_t>(port),
            // Bind to all interfaces by default — matches
            // upstream's ServerSocket, which uses INADDR_ANY.
            "0.0.0.0");
        m_AcceptorPimpl->server_port = acc->local_port();
        m_AcceptorPimpl->acceptor    = std::move(acc);
        return 0;
    } catch (...) {
        return -1;
    }
}

ClientSocket::Pointer ServerSocket::WaitForConnection(unsigned long msec) {
    if (!m_AcceptorPimpl->acceptor) return ClientSocket::Pointer();

    auto fut = m_AcceptorPimpl->acceptor->accept();
    if (msec != 0) {
        if (!fut.wait_for(std::chrono::milliseconds(msec))) {
            fut.cancel();
            return ClientSocket::Pointer();
        }
    }
    std::unique_ptr<oigtl::transport::Connection> conn;
    try {
        conn = fut.get();
    } catch (...) {
        return ClientSocket::Pointer();
    }
    auto cs = ClientSocket::New();
    cs->AttachConnection(static_cast<void*>(&conn));
    return cs;
}

int ServerSocket::GetServerPort() {
    return m_AcceptorPimpl ? m_AcceptorPimpl->server_port : 0;
}

}  // namespace igtl
