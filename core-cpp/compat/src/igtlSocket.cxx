// igtlSocket.cxx — Socket facade implementation.
//
// State machine: the underlying Connection delivers bytes one framed
// IGTL message at a time. Upstream's Receive() wants to read an
// arbitrary byte count, which in practice is always exactly the
// 58-byte header or exactly `body_size` bytes of body. We buffer one
// reassembled (header+body) wire message at a time and serve bytes
// from it with a cursor, pulling the next message from the Connection
// when the buffer drains.
//
// CRC / version: the Incoming delivered by the framer has the header
// already parsed and verified. To hand the caller raw 58-byte header
// bytes, we re-pack using the parsed header fields + body — the
// computed CRC will match what was on the wire because the body hasn't
// changed.

#include "igtl/igtlSocket.h"

#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "oigtl/runtime/header.hpp"
#include "oigtl/transport/connection.hpp"
#include "oigtl/transport/errors.hpp"

namespace igtl {

struct Socket::Pimpl {
    std::unique_ptr<oigtl::transport::Connection> conn;

    // Reassembled wire bytes (header + body) for the current
    // framed message being served to the caller via Receive().
    std::vector<std::uint8_t> rx_stash;
    std::size_t               rx_cursor = 0;

    // 0 = block indefinitely. Stored in milliseconds.
    int receive_timeout_ms = 0;
    int send_timeout_ms    = 0;

    // Cached peer endpoint (so GetSocketAddressAndPort works after
    // close, matching upstream's behaviour of keeping the fields).
    std::string peer_address;
    int         peer_port = 0;

    // Fill rx_stash with the next reassembled framed message.
    // Returns true on success, sets `timed_out=true` on timeout,
    // returns false on peer close / error (and clears conn).
    bool PullNextMessage(bool& timed_out);
};

Socket::Socket() : m_Pimpl(new Pimpl) {}
Socket::~Socket() = default;

bool Socket::GetConnected() {
    return m_Pimpl && m_Pimpl->conn != nullptr;
}

void Socket::CloseSocket() {
    if (!m_Pimpl || !m_Pimpl->conn) return;
    try {
        m_Pimpl->conn->close().get();
    } catch (...) {
        // Idempotent close: swallow — socket is going away regardless.
    }
    m_Pimpl->conn.reset();
    m_Pimpl->rx_stash.clear();
    m_Pimpl->rx_cursor = 0;
}

void Socket::AttachConnection(void* connection_owner) {
    auto* uptr = static_cast<
        std::unique_ptr<oigtl::transport::Connection>*>(connection_owner);
    m_Pimpl->conn = std::move(*uptr);
    if (m_Pimpl->conn) {
        m_Pimpl->peer_address = m_Pimpl->conn->peer_address();
        m_Pimpl->peer_port    = m_Pimpl->conn->peer_port();
    }
}

int Socket::Send(const void* data, igtlUint64 length) {
    if (!m_Pimpl->conn) return 0;
    try {
        m_Pimpl->conn->send_sync(
            static_cast<const std::uint8_t*>(data),
            static_cast<std::size_t>(length));
        return 1;
    } catch (...) {
        // Treat any send failure as connection-dead, matching
        // upstream (whose Send returns 0 on EPIPE / ECONNRESET).
        m_Pimpl->conn.reset();
        return 0;
    }
}

bool Socket::Pimpl::PullNextMessage(bool& timed_out) {
    timed_out = false;
    std::chrono::milliseconds to{receive_timeout_ms > 0
                                 ? receive_timeout_ms
                                 : -1};
    oigtl::transport::Incoming inc;
    try {
        inc = conn->receive_sync(to);
    } catch (const oigtl::transport::TimeoutError&) {
        timed_out = true;
        return false;
    } catch (...) {
        conn.reset();
        return false;
    }

    // Re-pack the header from parsed fields + body. CRC is
    // recomputed over `inc.body`; since the body is unchanged
    // from the wire, the resulting bytes match what the peer sent.
    std::array<std::uint8_t, oigtl::runtime::kHeaderSize> hdr{};
    oigtl::runtime::pack_header(hdr,
                                inc.header.version,
                                inc.header.type_id,
                                inc.header.device_name,
                                inc.header.timestamp,
                                inc.body.data(),
                                inc.body.size());

    rx_stash.clear();
    rx_stash.reserve(oigtl::runtime::kHeaderSize + inc.body.size());
    rx_stash.insert(rx_stash.end(), hdr.begin(), hdr.end());
    rx_stash.insert(rx_stash.end(),
                    inc.body.begin(), inc.body.end());
    rx_cursor = 0;
    return true;
}

igtlUint64 Socket::Receive(void* data,
                           igtlUint64 length,
                           bool& timeout,
                           int readFully) {
    timeout = false;
    if (!m_Pimpl->conn) return 0;
    auto* out = static_cast<std::uint8_t*>(data);
    igtlUint64 total = 0;

    while (total < length) {
        if (m_Pimpl->rx_cursor >= m_Pimpl->rx_stash.size()) {
            if (!m_Pimpl->PullNextMessage(timeout)) {
                // On timeout / error, hand back whatever we've
                // already copied. Upstream returns 0 on both
                // timeout and error — but a partial readFully=0
                // read should report what it has.
                return readFully ? 0 : total;
            }
        }
        igtlUint64 avail =
            m_Pimpl->rx_stash.size() - m_Pimpl->rx_cursor;
        igtlUint64 want = length - total;
        igtlUint64 take = avail < want ? avail : want;
        std::memcpy(out + total,
                    m_Pimpl->rx_stash.data() + m_Pimpl->rx_cursor,
                    static_cast<std::size_t>(take));
        m_Pimpl->rx_cursor += static_cast<std::size_t>(take);
        total              += take;
        if (!readFully) break;
    }
    return total;
}

int Socket::Skip(igtlUint64 length, int skipFully) {
    if (!m_Pimpl->conn) return 0;
    igtlUint64 remaining = length;
    while (remaining > 0) {
        if (m_Pimpl->rx_cursor >= m_Pimpl->rx_stash.size()) {
            bool to = false;
            if (!m_Pimpl->PullNextMessage(to)) return 0;
        }
        igtlUint64 avail =
            m_Pimpl->rx_stash.size() - m_Pimpl->rx_cursor;
        igtlUint64 take = avail < remaining ? avail : remaining;
        m_Pimpl->rx_cursor += static_cast<std::size_t>(take);
        remaining          -= take;
        if (!skipFully) break;
    }
    return 1;
}

int Socket::SetTimeout(int timeout) {
    m_Pimpl->receive_timeout_ms = timeout;
    m_Pimpl->send_timeout_ms    = timeout;
    return 0;
}

int Socket::SetReceiveTimeout(int timeout) {
    m_Pimpl->receive_timeout_ms = timeout;
    return 0;
}

int Socket::SetSendTimeout(int timeout) {
    m_Pimpl->send_timeout_ms = timeout;
    return 0;
}

int Socket::SetReceiveBlocking(int sw) {
    // sw=1 → non-blocking: minimum timeout. sw=0 → clear.
    m_Pimpl->receive_timeout_ms = sw ? 1 : 0;
    return 0;
}

int Socket::SetSendBlocking(int sw) {
    m_Pimpl->send_timeout_ms = sw ? 1 : 0;
    return 0;
}

int Socket::GetSocketAddressAndPort(std::string& address, int& port) {
    if (!m_Pimpl->conn && m_Pimpl->peer_address.empty()) return 0;
    address = m_Pimpl->peer_address;
    port    = m_Pimpl->peer_port;
    return 1;
}

}  // namespace igtl
