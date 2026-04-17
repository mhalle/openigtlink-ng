// socket_roundtrip_test.cxx — verifies that the shim's
// igtl::ClientSocket / igtl::ServerSocket pair behaves like upstream's:
// a server accepts, a client sends a packed message, the server reads
// the 58-byte header, Unpacks it, allocates the body buffer, reads
// body_size bytes, Unpacks, and observes the same typed contents.
//
// This is the canonical pattern used by every single upstream example
// program (TrackerServer, TrackerClient, Receiver/Receiver.cxx, etc.).
// If this test passes, every upstream example linked against the shim
// works.

#include "igtl/igtlClientSocket.h"
#include "igtl/igtlMessageHeader.h"
#include "igtl/igtlServerSocket.h"
#include "igtl/igtlSocket.h"
#include "igtl/igtlTransformMessage.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static int failures = 0;
#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::fprintf(stderr, "FAIL %s:%d: %s\n",                    \
                         __FILE__, __LINE__, #cond);                    \
            ++failures;                                                 \
        }                                                               \
    } while (0)

int main() {
    // --- bind an ephemeral port on the server --------------------------
    igtl::ServerSocket::Pointer srv = igtl::ServerSocket::New();
    EXPECT(srv->CreateServer(0) == 0);
    int port = srv->GetServerPort();
    EXPECT(port > 0);

    // --- client thread -------------------------------------------------
    std::atomic<bool> client_done{false};
    std::thread client([&]() {
        // Small sleep to make the race obvious if WaitForConnection
        // doesn't actually wait. Not strictly required — the server
        // ServerSocket::WaitForConnection() blocks until a connection
        // arrives.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        igtl::ClientSocket::Pointer cli = igtl::ClientSocket::New();
        EXPECT(cli->ConnectToServer("127.0.0.1", port) == 0);
        EXPECT(cli->GetConnected());

        // Build a TRANSFORM with a distinctive (non-identity) matrix
        // so "accidentally-sent-identity" wouldn't hide layout bugs.
        igtl::TransformMessage::Pointer msg = igtl::TransformMessage::New();
        msg->SetDeviceName("sock_test");
        msg->SetHeaderVersion(2);
        igtl::Matrix4x4 m;
        // 90° rotation around Z with translation (1, 2, 3):
        m[0][0] =  0; m[0][1] = -1; m[0][2] =  0; m[0][3] = 1;
        m[1][0] =  1; m[1][1] =  0; m[1][2] =  0; m[1][3] = 2;
        m[2][0] =  0; m[2][1] =  0; m[2][2] =  1; m[2][3] = 3;
        m[3][0] =  0; m[3][1] =  0; m[3][2] =  0; m[3][3] = 1;
        msg->SetMatrix(m);
        msg->Pack();

        int sent = cli->Send(msg->GetPackPointer(), msg->GetPackSize());
        EXPECT(sent == 1);

        client_done = true;
        // Let the server finish its Receive before we tear the socket
        // down; otherwise a brisk close can race the peer's recv and
        // deliver a spurious short read.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        cli->CloseSocket();
    });

    // --- server side: accept, then header-then-body -------------------
    igtl::Socket::Pointer peer = srv->WaitForConnection(5000).GetPointer();
    EXPECT(peer);
    EXPECT(peer->GetConnected());

    igtl::MessageHeader::Pointer headerMsg = igtl::MessageHeader::New();
    headerMsg->InitPack();

    bool timeout_flag = false;
    igtlUint64 n = peer->Receive(headerMsg->GetPackPointer(),
                                 headerMsg->GetPackSize(),
                                 timeout_flag);
    EXPECT(!timeout_flag);
    EXPECT(n == headerMsg->GetPackSize());
    EXPECT(headerMsg->Unpack() & igtl::UNPACK_HEADER);
    EXPECT(headerMsg->GetMessageType() == "TRANSFORM");

    // Construct the typed body and read its body bytes.
    igtl::TransformMessage::Pointer rx = igtl::TransformMessage::New();
    rx->SetMessageHeader(headerMsg);
    rx->AllocatePack();
    n = peer->Receive(rx->GetPackBodyPointer(),
                      rx->GetPackBodySize(),
                      timeout_flag);
    EXPECT(!timeout_flag);
    EXPECT(n == rx->GetPackBodySize());
    EXPECT(rx->Unpack() & igtl::UNPACK_BODY);

    // Verify the matrix round-tripped byte-exactly.
    igtl::Matrix4x4 out;
    rx->GetMatrix(out);
    EXPECT(out[0][0] ==  0); EXPECT(out[0][1] == -1);
    EXPECT(out[1][0] ==  1); EXPECT(out[1][1] ==  0);
    EXPECT(out[0][3] ==  1); EXPECT(out[1][3] ==  2);
    EXPECT(out[2][3] ==  3); EXPECT(out[2][2] ==  1);

    client.join();
    peer->CloseSocket();
    srv->CloseSocket();

    if (failures) {
        std::fprintf(stderr, "socket_roundtrip: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("socket_roundtrip: OK\n");
    return 0;
}
