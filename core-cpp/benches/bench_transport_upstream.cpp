// bench_transport_upstream — TRANSFORM throughput through
// upstream's ClientSocket/ServerSocket + TransformMessage APIs.
//
// Same scenario as bench_transport_ours: accept one connection,
// stream N v1 TRANSFORMs, count arrivals. Runs the server on a
// background thread using upstream's blocking Receive().

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#include "igtlClientSocket.h"
#include "igtlMessageHeader.h"
#include "igtlServerSocket.h"
#include "igtlTransformMessage.h"

namespace {

constexpr int kHeaderSize = 58;  // IGTL_HEADER_SIZE

void receiver_thread(int port, int expected, std::atomic<int>& done) {
    igtl::ServerSocket::Pointer sock = igtl::ServerSocket::New();
    sock->CreateServer(port);

    igtl::Socket::Pointer peer;
    while (!peer) {
        peer = sock->WaitForConnection(100);
    }

    auto header = igtl::MessageHeader::New();
    bool timeout_flag = false;
    while (done.load() < expected) {
        header->InitPack();
        auto r = peer->Receive(header->GetPackPointer(),
                               header->GetPackSize(),
                               timeout_flag);
        if (r == 0) break;
        header->Unpack();

        const std::size_t body_size = header->GetBodySizeToRead();
        if (body_size > 0) {
            std::vector<char> buf(body_size);
            r = peer->Receive(buf.data(), body_size, timeout_flag);
            if (r == 0) break;
        }
        ++done;
    }
    peer->CloseSocket();
    sock->CloseSocket();
}

}  // namespace

int main(int argc, char** argv) {
    const int N = (argc > 1) ? std::atoi(argv[1]) : 50000;

    // Upstream doesn't let us bind port 0 cleanly for discovery,
    // so pick a high port and hope it's free.
    const int port = 18946;

    std::atomic<int> done{0};
    std::thread recv(receiver_thread, port, N + 100, std::ref(done));

    // Give the server a beat to bind.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto client = igtl::ClientSocket::New();
    if (client->ConnectToServer("127.0.0.1", port) != 0) {
        std::fprintf(stderr, "connect failed\n");
        return 1;
    }

    auto msg = igtl::TransformMessage::New();
    msg->SetDeviceName("Bench");
    igtl::Matrix4x4 m;
    igtl::IdentityMatrix(m);
    msg->SetMatrix(m);
    msg->Pack();

    // Warmup
    for (int i = 0; i < 100; ++i) {
        client->Send(msg->GetPackPointer(), msg->GetPackSize());
    }
    while (done.load() < 100) std::this_thread::sleep_for(
        std::chrono::milliseconds(1));
    done = 0;

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        client->Send(msg->GetPackPointer(), msg->GetPackSize());
    }
    while (done.load() < N) std::this_thread::sleep_for(
        std::chrono::microseconds(100));
    const auto t1 = std::chrono::steady_clock::now();

    const double secs = std::chrono::duration<double>(t1 - t0).count();
    const double rate = N / secs;
    // v1 TRANSFORM on wire: 58 (header) + 48 (body) = 106 bytes
    const double bytes = N * 106.0;
    const double mbps = bytes / secs / (1024 * 1024);

    std::printf("upstream: N=%d  %.3f s  %.0f msg/s  %.1f MB/s\n",
                N, secs, rate, mbps);

    client->CloseSocket();
    recv.join();
    return 0;
}
