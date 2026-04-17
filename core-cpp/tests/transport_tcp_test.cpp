// Phase 2 acceptance: drive the TCP backend end-to-end.
//
// Tests:
//   1. 100-message round-trip (client→server→echo→client), byte-exact.
//   2. capability() carries tcp.peer_address / tcp.peer_port.
//   3. Graceful close: peer FIN → pending receive() resolves with
//      ConnectionClosedError.
//   4. Cancellation: fut.cancel() on a pending receive() resolves
//      with OperationCancelledError.
//   5. Malformed CRC surfaces via the Future (inherited from Phase 1).
//   6. Connection to refused port surfaces via the Future.

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "oigtl/runtime/error.hpp"
#include "oigtl/runtime/header.hpp"
#include "oigtl/transport/connection.hpp"
#include "oigtl/transport/errors.hpp"
#include "oigtl/transport/future.hpp"
#include "oigtl/transport/tcp.hpp"

namespace {

int g_fail_count = 0;

#define REQUIRE(expr) do {                                            \
    if (!(expr)) {                                                    \
        std::fprintf(stderr, "  FAIL: %s:%d  %s\n",                   \
                     __FILE__, __LINE__, #expr);                      \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

std::vector<std::uint8_t>
make_wire(const std::string& type_id,
          const std::string& device_name,
          std::uint64_t timestamp,
          const std::vector<std::uint8_t>& body) {
    std::array<std::uint8_t, oigtl::runtime::kHeaderSize> hdr{};
    oigtl::runtime::pack_header(hdr, 1, type_id, device_name, timestamp,
                                body.data(), body.size());
    std::vector<std::uint8_t> wire;
    wire.reserve(hdr.size() + body.size());
    wire.insert(wire.end(), hdr.begin(), hdr.end());
    wire.insert(wire.end(), body.begin(), body.end());
    return wire;
}

void test_roundtrip_100_messages() {
    std::fprintf(stderr, "test_roundtrip_100_messages\n");
    namespace ot = oigtl::transport;

    auto acceptor = ot::tcp::listen(0);
    const auto port = acceptor->local_port();
    REQUIRE(port != 0);

    // Echo-server thread: accept one connection, echo each message
    // back, stop when the client closes.
    std::thread server([&]() {
        try {
            auto peer_fut = acceptor->accept();
            auto peer = peer_fut.get();
            for (;;) {
                auto inc = peer->receive().get();
                auto wire = make_wire(
                    inc.header.type_id, inc.header.device_name,
                    inc.header.timestamp, inc.body);
                peer->send(wire).get();
            }
        } catch (const ot::ConnectionClosedError&) {
            // expected on client shutdown
        } catch (...) {
            std::fprintf(stderr, "  server unexpected exception\n");
            ++g_fail_count;
        }
    });

    auto client = ot::tcp::connect("127.0.0.1", port).get();

    REQUIRE(client->capability("tcp.peer_address").value_or("") ==
            "127.0.0.1");
    REQUIRE(client->capability("tcp.peer_port").value_or("") ==
            std::to_string(port));
    REQUIRE(client->capability("framer").value_or("") == "v3");

    std::mt19937 rng(0xBEEF);
    for (int i = 0; i < 100; ++i) {
        std::size_t n = (rng() % 64) + 1;
        std::vector<std::uint8_t> body(n);
        for (auto& b : body) b = static_cast<std::uint8_t>(rng());
        auto wire = make_wire("STATUS", "cli",
                              static_cast<std::uint64_t>(i), body);
        client->send(wire).get();
        auto echo = client->receive().get();
        std::vector<std::uint8_t> sent_body(
            wire.begin() + oigtl::runtime::kHeaderSize, wire.end());
        REQUIRE(echo.body == sent_body);
        REQUIRE(echo.header.timestamp == static_cast<std::uint64_t>(i));
    }

    client->close().get();
    server.join();
}

void test_peer_fin_resolves_receive() {
    std::fprintf(stderr, "test_peer_fin_resolves_receive\n");
    namespace ot = oigtl::transport;

    auto acceptor = ot::tcp::listen(0);
    const auto port = acceptor->local_port();

    std::thread server([&]() {
        try {
            auto peer = acceptor->accept().get();
            // Close immediately; the client's pending receive() must
            // resolve with ConnectionClosedError.
            peer->close().get();
        } catch (...) { ++g_fail_count; }
    });

    auto client = ot::tcp::connect("127.0.0.1", port).get();
    auto rx = client->receive();

    bool caught = false;
    try { (void)rx.get(); }
    catch (const ot::ConnectionClosedError&) { caught = true; }
    REQUIRE(caught);

    server.join();
}

void test_cancellation() {
    std::fprintf(stderr, "test_cancellation\n");
    namespace ot = oigtl::transport;

    auto acceptor = ot::tcp::listen(0);
    const auto port = acceptor->local_port();

    std::thread server([&]() {
        try {
            auto peer = acceptor->accept().get();
            // Never send; just hold until the client disconnects.
            (void)peer->receive().get();
        } catch (...) { /* expected on teardown */ }
    });

    auto client = ot::tcp::connect("127.0.0.1", port).get();
    auto rx = client->receive();
    REQUIRE(!rx.wait_for(std::chrono::milliseconds(20)));

    rx.cancel();
    bool caught = false;
    try { (void)rx.get(); }
    catch (const ot::OperationCancelledError&) { caught = true; }
    REQUIRE(caught);

    client->close().get();
    server.join();
}

void test_connect_refused() {
    std::fprintf(stderr, "test_connect_refused\n");
    namespace ot = oigtl::transport;

    // Port 1 is almost always refused/unprivileged-only.
    auto fut = ot::tcp::connect("127.0.0.1", 1);
    bool caught = false;
    try { (void)fut.get(); }
    catch (const ot::ConnectionClosedError&) { caught = true; }
    REQUIRE(caught);
}

void test_malformed_crc_surfaces() {
    std::fprintf(stderr, "test_malformed_crc_surfaces\n");
    namespace ot = oigtl::transport;

    auto acceptor = ot::tcp::listen(0);
    const auto port = acceptor->local_port();

    std::thread server([&]() {
        try {
            auto peer = acceptor->accept().get();
            std::vector<std::uint8_t> body = {1, 2, 3};
            auto wire = make_wire("STATUS", "srv", 0, body);
            wire[oigtl::runtime::kHeaderSize] ^= 0xFF;  // corrupt CRC
            peer->send(wire).get();
            (void)peer->receive();  // keep peer alive briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } catch (...) { /* teardown */ }
    });

    auto client = ot::tcp::connect("127.0.0.1", port).get();
    bool caught = false;
    try { (void)client->receive().get(); }
    catch (const oigtl::error::ProtocolError&) { caught = true; }
    REQUIRE(caught);

    client->close().get();
    server.join();
}

}  // namespace

int main() {
    test_roundtrip_100_messages();
    test_peer_fin_resolves_receive();
    test_cancellation();
    test_connect_refused();
    test_malformed_crc_surfaces();

    if (g_fail_count == 0) {
        std::fprintf(stderr, "transport_tcp_test: all passed\n");
        return 0;
    }
    std::fprintf(stderr, "transport_tcp_test: %d failure(s)\n",
                 g_fail_count);
    return 1;
}
