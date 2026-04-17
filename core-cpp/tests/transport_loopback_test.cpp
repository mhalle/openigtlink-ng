// Phase 1 acceptance: exercise the Connection / Framer / Future
// contract through the in-memory loopback pair.
//
// Acceptance criteria (from TRANSPORT_PLAN.md §Phase 1):
//   1. 1000 messages pushed through, byte-for-byte preserved.
//   2. Future<T> semantics: await, fulfill, cancel, error propagation.
//   3. capability() returns loopback-specific keys.
//   4. Incoming.body matches wire[kHeaderSize:].
//
// No GoogleTest dependency — matches the hand-rolled REQUIRE style
// used by the existing tests.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "oigtl/runtime/error.hpp"
#include "oigtl/runtime/header.hpp"
#include "oigtl/transport/connection.hpp"
#include "oigtl/transport/errors.hpp"
#include "oigtl/transport/framer.hpp"
#include "oigtl/transport/future.hpp"

namespace {

int g_fail_count = 0;

#define REQUIRE(expr) do {                                            \
    if (!(expr)) {                                                    \
        std::fprintf(stderr, "  FAIL: %s:%d  %s\n",                   \
                     __FILE__, __LINE__, #expr);                      \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

// Build a fully-framed wire message: 58-byte header + N body bytes.
// Uses pack_header so the CRC is correct.
std::vector<std::uint8_t>
make_wire(const std::string& type_id,
          const std::string& device_name,
          std::uint64_t timestamp,
          const std::vector<std::uint8_t>& body,
          std::uint16_t version = 1) {
    std::array<std::uint8_t, oigtl::runtime::kHeaderSize> hdr{};
    oigtl::runtime::pack_header(hdr, version, type_id, device_name,
                                timestamp, body.data(), body.size());
    std::vector<std::uint8_t> wire;
    wire.reserve(hdr.size() + body.size());
    wire.insert(wire.end(), hdr.begin(), hdr.end());
    wire.insert(wire.end(), body.begin(), body.end());
    return wire;
}

void test_capability_keys() {
    std::fprintf(stderr, "test_capability_keys\n");
    auto [a, b] = oigtl::transport::make_loopback_pair();

    REQUIRE(a->capability("loopback.side").value_or("") == "a");
    REQUIRE(b->capability("loopback.side").value_or("") == "b");
    REQUIRE(a->capability("framer").value_or("") == "v3");
    REQUIRE(!a->capability("nonexistent").has_value());

    // Same peer_id on both sides.
    REQUIRE(a->capability("loopback.peer_id")
            == b->capability("loopback.peer_id"));

    REQUIRE(a->peer_address() == "loopback");
    REQUIRE(a->negotiated_version() == 0);
}

void test_future_semantics() {
    std::fprintf(stderr, "test_future_semantics\n");

    // await + fulfill
    {
        oigtl::transport::Promise<int> p;
        auto fut = p.get_future();
        p.set_value(42);
        REQUIRE(fut.get() == 42);
    }

    // error propagation
    {
        oigtl::transport::Promise<int> p;
        auto fut = p.get_future();
        p.set_exception(std::make_exception_ptr(
            std::runtime_error("boom")));
        bool caught = false;
        try { (void)fut.get(); }
        catch (const std::runtime_error& e) {
            caught = std::string(e.what()) == "boom";
        }
        REQUIRE(caught);
    }

    // cancel before fulfill → OperationCancelledError
    {
        oigtl::transport::Promise<int> p;
        auto fut = p.get_future();
        fut.cancel();
        bool caught = false;
        try { (void)fut.get(); }
        catch (const oigtl::transport::OperationCancelledError&) {
            caught = true;
        }
        REQUIRE(caught);
        REQUIRE(p.cancel_requested());
    }

    // then() on value
    {
        oigtl::transport::Promise<int> p;
        auto fut2 = p.get_future().then([](int x) { return x * 2; });
        p.set_value(21);
        REQUIRE(fut2.get() == 42);
    }

    // then() on void producer
    {
        oigtl::transport::Promise<void> p;
        int result = 0;
        auto fut2 = p.get_future().then([&] { result = 7; return result; });
        p.set_value();
        REQUIRE(fut2.get() == 7);
    }

    // then() propagates exception past the continuation
    {
        oigtl::transport::Promise<int> p;
        auto fut2 = p.get_future().then([](int x) { return x + 1; });
        p.set_exception(std::make_exception_ptr(
            std::runtime_error("upstream fail")));
        bool caught = false;
        try { (void)fut2.get(); }
        catch (const std::runtime_error&) { caught = true; }
        REQUIRE(caught);
    }

    // async fulfill from another thread
    {
        oigtl::transport::Promise<int> p;
        auto fut = p.get_future();
        std::thread t([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            p.set_value(99);
        });
        REQUIRE(fut.get() == 99);
        t.join();
    }

    // wait_for timeout
    {
        oigtl::transport::Promise<int> p;
        auto fut = p.get_future();
        bool ready = fut.wait_for(std::chrono::milliseconds(5));
        REQUIRE(!ready);
        p.set_value(0);
        REQUIRE(fut.wait_for(std::chrono::milliseconds(5)));
    }
}

void test_single_message_roundtrip() {
    std::fprintf(stderr, "test_single_message_roundtrip\n");
    auto [a, b] = oigtl::transport::make_loopback_pair();

    std::vector<std::uint8_t> body = {1, 2, 3, 4, 5};
    auto wire = make_wire("STATUS", "dev", 12345, body);

    // send on A, receive on B
    a->send(wire).get();
    auto inc = b->receive().get();

    REQUIRE(inc.header.type_id == "STATUS");
    REQUIRE(inc.header.device_name == "dev");
    REQUIRE(inc.header.body_size == body.size());
    REQUIRE(inc.body == body);

    // Property 2 check: body matches wire[kHeaderSize:]
    std::vector<std::uint8_t> wire_body(
        wire.begin() + oigtl::runtime::kHeaderSize, wire.end());
    REQUIRE(inc.body == wire_body);
}

void test_thousand_messages_byte_exact() {
    std::fprintf(stderr, "test_thousand_messages_byte_exact (1000 msgs)\n");
    auto [a, b] = oigtl::transport::make_loopback_pair();

    std::mt19937 rng(0xC0FFEE);
    std::vector<std::vector<std::uint8_t>> sent_wires;
    sent_wires.reserve(1000);

    // Send 1000 messages with varying body sizes, alternating directions.
    for (int i = 0; i < 1000; ++i) {
        std::size_t body_size = (rng() % 128) + 1;
        std::vector<std::uint8_t> body(body_size);
        for (auto& byte : body) byte = static_cast<std::uint8_t>(rng());

        auto wire = make_wire("STATUS", "src",
                              static_cast<std::uint64_t>(i), body);
        sent_wires.push_back(wire);
        a->send(wire).get();
    }

    // Drain all on B.
    for (int i = 0; i < 1000; ++i) {
        auto inc = b->receive().get();
        const auto& wire = sent_wires[i];
        std::vector<std::uint8_t> expected_body(
            wire.begin() + oigtl::runtime::kHeaderSize, wire.end());
        REQUIRE(inc.body == expected_body);
        REQUIRE(inc.header.body_size == expected_body.size());
        REQUIRE(inc.header.timestamp == static_cast<std::uint64_t>(i));
    }
}

void test_close_delivers_closed_error() {
    std::fprintf(stderr, "test_close_delivers_closed_error\n");
    auto [a, b] = oigtl::transport::make_loopback_pair();

    // Park a receive on B, then close A. B's receive should resolve
    // with ConnectionClosedError.
    auto rx = b->receive();
    REQUIRE(!rx.wait_for(std::chrono::milliseconds(5)));

    a->close().get();

    bool caught = false;
    try { (void)rx.get(); }
    catch (const oigtl::transport::ConnectionClosedError&) { caught = true; }
    REQUIRE(caught);

    // Subsequent send on a closed connection also errors.
    std::vector<std::uint8_t> body = {0};
    auto wire = make_wire("STATUS", "d", 0, body);
    bool send_err = false;
    try { a->send(wire).get(); }
    catch (const oigtl::transport::ConnectionClosedError&) {
        send_err = true;
    }
    REQUIRE(send_err);
}

void test_bidirectional() {
    std::fprintf(stderr, "test_bidirectional\n");
    auto [a, b] = oigtl::transport::make_loopback_pair();

    std::vector<std::uint8_t> body_a = {0xAA, 0xBB};
    std::vector<std::uint8_t> body_b = {0xCC, 0xDD, 0xEE};

    auto wa = make_wire("STATUS", "a", 1, body_a);
    auto wb = make_wire("STATUS", "b", 2, body_b);

    a->send(wa).get();
    b->send(wb).get();

    auto rb = b->receive().get();
    auto ra = a->receive().get();

    REQUIRE(rb.body == body_a);
    REQUIRE(rb.header.device_name == "a");
    REQUIRE(ra.body == body_b);
    REQUIRE(ra.header.device_name == "b");
}

void test_malformed_surfaces_through_future() {
    std::fprintf(stderr, "test_malformed_surfaces_through_future\n");
    auto [a, b] = oigtl::transport::make_loopback_pair();

    // Send a wire message, then corrupt the CRC by flipping a body
    // byte en route (we build a second wire by hand with a bogus
    // CRC — can't easily reach into the pair's inbox, so instead
    // we pack a good message, then flip one body byte in the vector
    // before sending).
    std::vector<std::uint8_t> body = {1, 2, 3};
    auto wire = make_wire("STATUS", "d", 0, body);
    wire[oigtl::runtime::kHeaderSize] ^= 0xFF;  // corrupt body byte 0

    a->send(wire).get();
    bool caught = false;
    try { (void)b->receive().get(); }
    catch (const oigtl::error::ProtocolError&) { caught = true; }
    REQUIRE(caught);
}

}  // namespace

int main() {
    test_capability_keys();
    test_future_semantics();
    test_single_message_roundtrip();
    test_thousand_messages_byte_exact();
    test_close_delivers_closed_error();
    test_bidirectional();
    test_malformed_surfaces_through_future();

    if (g_fail_count == 0) {
        std::fprintf(stderr, "transport_loopback_test: all passed\n");
        return 0;
    }
    std::fprintf(stderr, "transport_loopback_test: %d failure(s)\n",
                 g_fail_count);
    return 1;
}
