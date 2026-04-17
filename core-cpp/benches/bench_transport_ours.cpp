// bench_transport_ours — TRANSFORM throughput using our API.
//
// In-process: Server accepts one peer; Client streams N v1
// TRANSFORM messages as fast as send() will go; server counts
// arrivals via on<Transform>. Loopback TCP.
//
// Fair-comparison knobs (match bench_transport_upstream):
//   - v1 messages (no extended header / no metadata)
//   - body-only TRANSFORM = 48 bytes content → 106 bytes on wire
//   - defaults; no TCP_NODELAY tuning on either side

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "oigtl/client.hpp"
#include "oigtl/messages/transform.hpp"
#include "oigtl/server.hpp"

namespace om = oigtl::messages;

int main(int argc, char** argv) {
    const int N = (argc > 1) ? std::atoi(argv[1]) : 50000;

    auto server = oigtl::Server::listen(0, {.bind_address = "127.0.0.1"});
    const auto port = server.local_port();

    std::atomic<int> received{0};
    server.on<om::Transform>(
        [&](oigtl::Client&, const oigtl::Envelope<om::Transform>&) {
            ++received;
        });

    std::thread server_runner([&] { server.run(); });

    // Let server hit accept()
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto client = oigtl::Client::connect("127.0.0.1", port);

    // Warmup
    om::Transform xform;
    xform.matrix = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    oigtl::Envelope<om::Transform> env;
    env.version = 1;
    env.device_name = "Bench";
    env.body = xform;
    for (int i = 0; i < 100; ++i) client.send(env);

    // Wait for warmup to drain
    while (received.load() < 100) std::this_thread::sleep_for(
        std::chrono::milliseconds(1));
    received = 0;

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) client.send(env);

    // Wait for server to see them all
    while (received.load() < N) std::this_thread::sleep_for(
        std::chrono::microseconds(100));
    const auto t1 = std::chrono::steady_clock::now();

    const double secs = std::chrono::duration<double>(t1 - t0).count();
    const double rate = N / secs;
    // v1 TRANSFORM on wire: 58 (header) + 48 (body) = 106 bytes
    const double bytes = N * 106.0;
    const double mbps = bytes / secs / (1024 * 1024);

    std::printf("ours:    N=%d  %.3f s  %.0f msg/s  %.1f MB/s\n",
                N, secs, rate, mbps);

    client.close();
    server.stop();
    server_runner.join();
    return 0;
}
