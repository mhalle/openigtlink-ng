// compat_hello.cpp — minimum-viable compat-mode example.
//
// Uses only the upstream `igtl::` API. The code here would compile
// verbatim against the original libOpenIGTLink; only the link line
// differs. Demonstrates:
//
//   - igtl::ServerSocket + igtl::ClientSocket end-to-end
//   - packing a TransformMessage with a non-identity matrix
//   - the upstream receive pattern: header, Unpack, allocate body,
//     receive body, Unpack(1)
//   - GetDeviceType-based dispatch (the string "TRANSFORM")
//   - TimeStamp object round-trip
//
// Build:
//   Standalone:
//     c++ -std=c++17 compat_hello.cpp $(pkg-config --cflags --libs oigtl) -o compat_hello
//
//   In-tree (no install needed):
//     cmake --build build --target compat_hello
//
// Run:
//   ./compat_hello

#include "igtlClientSocket.h"
#include "igtlMessageHeader.h"
#include "igtlOSUtil.h"
#include "igtlServerSocket.h"
#include "igtlStatusMessage.h"
#include "igtlTimeStamp.h"
#include "igtlTransformMessage.h"

#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>

namespace {

// Server-side logic: accept one client, read one TRANSFORM, reply
// with a STATUS.
void server_thread(int port, std::atomic<bool>& ready) {
    igtl::ServerSocket::Pointer srv = igtl::ServerSocket::New();
    if (srv->CreateServer(port) != 0) {
        std::cerr << "[server] CreateServer failed\n";
        return;
    }
    ready = true;

    // Block up to 5s for a client.
    auto peer = srv->WaitForConnection(5000);
    if (!peer) { std::cerr << "[server] no client\n"; return; }

    // Upstream receive dance: header first, then body.
    auto hdr = igtl::MessageHeader::New();
    hdr->InitPack();
    bool timed_out = false;
    auto n = peer->Receive(hdr->GetPackPointer(),
                           hdr->GetPackSize(), timed_out);
    if (timed_out || n != hdr->GetPackSize()) {
        std::cerr << "[server] header read failed\n"; return;
    }
    hdr->Unpack();
    std::cout << "[server] got header: type='" << hdr->GetDeviceType()
              << "' device='" << hdr->GetDeviceName()
              << "' body_size=" << hdr->GetBodySizeToRead() << "\n";

    // Dispatch on type.
    if (std::strcmp(hdr->GetDeviceType(), "TRANSFORM") == 0) {
        auto tx = igtl::TransformMessage::New();
        tx->SetMessageHeader(hdr);
        tx->AllocatePack();
        peer->Receive(tx->GetPackBodyPointer(),
                      tx->GetPackBodySize(), timed_out);
        if (!(tx->Unpack(1) & igtl::MessageBase::UNPACK_BODY)) {
            std::cerr << "[server] TRANSFORM body unpack failed\n";
            return;
        }
        igtl::Matrix4x4 m;
        tx->GetMatrix(m);
        std::cout << "[server] TRANSFORM matrix diag = ("
                  << m[0][0] << "," << m[1][1] << "," << m[2][2]
                  << "), translation = ("
                  << m[0][3] << "," << m[1][3] << "," << m[2][3]
                  << ")\n";

        // Ack with STATUS.
        auto st = igtl::StatusMessage::New();
        st->SetDeviceName("server");
        st->SetCode(igtl::StatusMessage::STATUS_OK);
        st->SetSubCode(0);
        st->SetErrorName("None");
        st->SetStatusString("TRANSFORM received");
        st->Pack();
        peer->Send(st->GetPackPointer(), st->GetPackSize());
        std::cout << "[server] sent STATUS ack\n";
    }
    peer->CloseSocket();
    srv->CloseSocket();
}

// Client-side logic: connect, send one TRANSFORM, read the STATUS
// reply.
int client_logic(int port) {
    auto sock = igtl::ClientSocket::New();
    if (sock->ConnectToServer("127.0.0.1", port) != 0) {
        std::cerr << "[client] connect failed\n"; return 1;
    }

    // Build a non-identity TRANSFORM — 90° Z-rotation with a
    // translation. (Identity matrices hide column/row-major
    // layout bugs; picking something distinctive means a silent
    // failure becomes a loud failure.)
    auto tx = igtl::TransformMessage::New();
    tx->SetDeviceName("client");
    tx->SetHeaderVersion(2);
    igtl::Matrix4x4 m = {
        { 0, -1,  0,  10 },
        { 1,  0,  0,  20 },
        { 0,  0,  1,  30 },
        { 0,  0,  0,   1 },
    };
    tx->SetMatrix(m);

    // Timestamp with the current wall clock.
    auto ts = igtl::TimeStamp::New();
    ts->GetTime();
    tx->SetTimeStamp(ts);

    tx->Pack();
    int sent = sock->Send(tx->GetPackPointer(), tx->GetPackSize());
    std::cout << "[client] sent TRANSFORM (" << tx->GetPackSize()
              << " bytes), rc=" << sent << "\n";

    // Read STATUS reply.
    auto hdr = igtl::MessageHeader::New();
    hdr->InitPack();
    bool timed_out = false;
    sock->Receive(hdr->GetPackPointer(),
                  hdr->GetPackSize(), timed_out);
    hdr->Unpack();

    if (std::strcmp(hdr->GetDeviceType(), "STATUS") == 0) {
        auto st = igtl::StatusMessage::New();
        st->SetMessageHeader(hdr);
        st->AllocatePack();
        sock->Receive(st->GetPackBodyPointer(),
                      st->GetPackBodySize(), timed_out);
        st->Unpack(1);
        std::cout << "[client] got STATUS code=" << st->GetCode()
                  << " msg='" << st->GetStatusString() << "'\n";
    }
    sock->CloseSocket();
    return 0;
}

}  // namespace

int main() {
    const int port = 18946;

    std::atomic<bool> server_ready{false};
    std::thread srv([&]() { server_thread(port, server_ready); });

    // Wait briefly for the server to bind. A real program would
    // handle this with a retry loop.
    while (!server_ready) {
        igtl::Sleep(10);
    }
    igtl::Sleep(50);  // give the accept loop a moment

    int rc = client_logic(port);
    srv.join();
    return rc;
}
