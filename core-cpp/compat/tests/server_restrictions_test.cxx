// server_restrictions_test.cxx — exercises the opt-in access
// restrictions on igtl::ServerSocket.
//
// Each subtest sets one restriction, verifies that out-of-policy
// peers are refused, and that in-policy peers still work.
// Restrictions tested:
//   1. RestrictToThisMachineOnly — loopback-only
//   2. AllowPeerRange — positive match via loopback-covering range
//   3. SetMaxSimultaneousClients — 2nd connection over the cap
//      is refused while an in-policy first is still open
//   4. SetMaxMessageSizeBytes — oversized body_size rejected before
//      the body is read
//
// Not tested directly: idle-timeout (requires multi-second waits,
// deferred to a slower test harness).

#include "igtl/igtlClientSocket.h"
#include "igtl/igtlMessageHeader.h"
#include "igtl/igtlServerSocket.h"
#include "igtl/igtlTransformMessage.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {

int failures = 0;
#define EXPECT(c)                                                       \
    do {                                                                \
        if (!(c)) {                                                     \
            std::fprintf(stderr, "FAIL %s:%d: %s\n",                    \
                         __FILE__, __LINE__, #c);                       \
            ++failures;                                                 \
        }                                                               \
    } while (0)

// Pack a minimum-viable TRANSFORM for send tests.
std::vector<std::uint8_t> pack_transform() {
    auto tx = igtl::TransformMessage::New();
    tx->SetDeviceName("t");
    tx->SetHeaderVersion(2);
    igtl::Matrix4x4 m;
    igtl::IdentityMatrix(m);
    tx->SetMatrix(m);
    tx->Pack();
    auto* p = static_cast<std::uint8_t*>(tx->GetPackPointer());
    return std::vector<std::uint8_t>(p, p + tx->GetPackSize());
}

// Short helper: bring up a server with a mutator applied, try to
// connect from 127.0.0.1, return whether the connection was
// accepted (i.e. we could send a TRANSFORM and server saw it).
//
// `apply` runs between New() and CreateServer(), so the policy is
// present from the first accept.
bool connect_and_send_transform(
        std::function<void(igtl::ServerSocket::Pointer)> apply,
        std::chrono::milliseconds client_delay =
            std::chrono::milliseconds(0)) {
    auto srv = igtl::ServerSocket::New();
    apply(srv);
    if (srv->CreateServer(0) != 0) return false;
    const int port = srv->GetServerPort();

    std::atomic<bool> server_saw_transform{false};
    std::thread srvt([&] {
        auto peer = srv->WaitForConnection(2000);
        if (!peer) return;
        auto hdr = igtl::MessageHeader::New();
        hdr->InitPack();
        bool to = false;
        auto n = peer->Receive(hdr->GetPackPointer(),
                               hdr->GetPackSize(), to);
        if (n != hdr->GetPackSize()) return;
        hdr->Unpack();
        if (std::strcmp(hdr->GetDeviceType(), "TRANSFORM") == 0) {
            server_saw_transform = true;
        }
        peer->CloseSocket();
    });

    if (client_delay.count() > 0) {
        std::this_thread::sleep_for(client_delay);
    }

    auto cli = igtl::ClientSocket::New();
    const int rc = cli->ConnectToServer("127.0.0.1", port);
    if (rc == 0) {
        auto bytes = pack_transform();
        cli->Send(bytes.data(), bytes.size());
        // Give the server a moment to Receive.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        cli->CloseSocket();
    }

    srvt.join();
    srv->CloseSocket();
    return server_saw_transform.load();
}

}  // namespace

int main() {
    // ---------------------------------------------------------------
    // 1. RestrictToThisMachineOnly — loopback accepted.
    // ---------------------------------------------------------------
    {
        bool ok = connect_and_send_transform([](auto srv) {
            srv->RestrictToThisMachineOnly();
        });
        EXPECT(ok);
    }

    // ---------------------------------------------------------------
    // 2. AllowPeerRange spanning loopback — accepted.
    // ---------------------------------------------------------------
    {
        bool ok = connect_and_send_transform([](auto srv) {
            EXPECT(srv->AllowPeerRange("127.0.0.0", "127.255.255.255")
                   == 1);
        });
        EXPECT(ok);
    }

    // ---------------------------------------------------------------
    // 2b. AllowPeerRange NOT spanning loopback — refused.
    //     We model this as "build a server that only allows a
    //     never-used range, then try to connect from 127.0.0.1."
    //     The client connects at the TCP layer but the acceptor
    //     closes the socket before a framed message goes through;
    //     from the client's perspective the send succeeds (into
    //     the kernel buffer) but the peer never shows Receive.
    // ---------------------------------------------------------------
    {
        bool ok = connect_and_send_transform([](auto srv) {
            EXPECT(srv->AllowPeerRange("10.99.99.1", "10.99.99.254")
                   == 1);
        });
        EXPECT(!ok);  // policy should refuse loopback
    }

    // ---------------------------------------------------------------
    // 3. SetMaxSimultaneousClients(1).
    //
    // Server holds client 1 until signaled. While c1 is held,
    // c2 attempts to connect. The acceptor's policy check sees
    // active=1, max=1, refuses c2's accept (closes socket, re-arms
    // waiting on the server's next WaitForConnection). c2's
    // ConnectToServer itself returns 0 because the kernel
    // three-way handshake completed — max-concurrent refusal
    // is an app-layer policy, not kernel-level.
    // ---------------------------------------------------------------
    {
        auto srv = igtl::ServerSocket::New();
        srv->SetMaxSimultaneousClients(1);
        EXPECT(srv->CreateServer(0) == 0);
        const int port = srv->GetServerPort();

        std::atomic<int> accepted{0};
        std::thread srvt([&] {
            // Accept c1 — succeeds. Keep p1 alive so the active-
            // connection counter stays at 1 while we issue a
            // second accept.
            auto p1 = srv->WaitForConnection(1500);
            if (p1) ++accepted;
            // Second accept: should hit max-concurrent, get
            // refused by policy, and time out returning nullptr.
            auto p2 = srv->WaitForConnection(700);
            if (p2) { ++accepted; p2->CloseSocket(); }
            if (p1) p1->CloseSocket();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto c1 = igtl::ClientSocket::New();
        EXPECT(c1->ConnectToServer("127.0.0.1", port) == 0);

        // Give the server time to enter the second WaitForConnection
        // while p1 is still in scope.
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        auto c2 = igtl::ClientSocket::New();
        (void)c2->ConnectToServer("127.0.0.1", port);

        srvt.join();
        c1->CloseSocket();
        c2->CloseSocket();
        srv->CloseSocket();

        EXPECT(accepted.load() == 1);
    }

    // ---------------------------------------------------------------
    // 4. SetMaxMessageSizeBytes — an oversized TRANSFORM body is
    //    refused. We fake this by setting max=4 (smaller than any
    //    real IGTL body) so a legitimate TRANSFORM is itself
    //    refused.
    // ---------------------------------------------------------------
    {
        bool ok = connect_and_send_transform([](auto srv) {
            srv->SetMaxMessageSizeBytes(4);
        });
        EXPECT(!ok);  // framer rejects oversized body on server side
    }

    if (failures) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    std::printf("server_restrictions: OK\n");
    return 0;
}
