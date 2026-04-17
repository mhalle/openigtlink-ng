// Phase 4 acceptance: sync::block_on over the Future API.
//
// Tests:
//   1. block_on on a ready future returns the value.
//   2. block_on on a never-resolving future throws TimeoutError at
//      ~the deadline.
//   3. block_on propagates the Future's stored exception.
//   4. block_on over a real loopback receive() works end-to-end.
//   5. Two threads block_on two independent futures concurrently.
//   6. Void-Future form.

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "oigtl/runtime/header.hpp"
#include "oigtl/transport/connection.hpp"
#include "oigtl/transport/errors.hpp"
#include "oigtl/transport/future.hpp"
#include "oigtl/transport/sync.hpp"

namespace {

int g_fail_count = 0;

#define REQUIRE(expr) do {                                            \
    if (!(expr)) {                                                    \
        std::fprintf(stderr, "  FAIL: %s:%d  %s\n",                   \
                     __FILE__, __LINE__, #expr);                      \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

void test_ready_returns_value() {
    std::fprintf(stderr, "test_ready_returns_value\n");
    namespace ot = oigtl::transport;
    auto v = ot::sync::block_on(ot::make_ready_future(42),
                                std::chrono::milliseconds(10));
    REQUIRE(v == 42);
}

void test_timeout() {
    std::fprintf(stderr, "test_timeout\n");
    namespace ot = oigtl::transport;
    ot::Promise<int> p;
    auto fut = p.get_future();
    auto t0 = std::chrono::steady_clock::now();
    bool caught = false;
    try {
        (void)ot::sync::block_on(std::move(fut),
                                 std::chrono::milliseconds(50));
    } catch (const ot::TimeoutError&) {
        caught = true;
    }
    auto dt = std::chrono::steady_clock::now() - t0;
    REQUIRE(caught);
    // Roughly ~50 ms; wide tolerance to accommodate busy CI.
    REQUIRE(dt >= std::chrono::milliseconds(40));
    REQUIRE(dt < std::chrono::milliseconds(500));
}

void test_exception_propagates() {
    std::fprintf(stderr, "test_exception_propagates\n");
    namespace ot = oigtl::transport;
    ot::Promise<int> p;
    auto fut = p.get_future();
    p.set_exception(std::make_exception_ptr(
        std::runtime_error("planned failure")));
    bool caught = false;
    try {
        (void)ot::sync::block_on(std::move(fut),
                                 std::chrono::milliseconds(10));
    } catch (const std::runtime_error& e) {
        caught = std::string(e.what()) == "planned failure";
    }
    REQUIRE(caught);
}

void test_loopback_end_to_end() {
    std::fprintf(stderr, "test_loopback_end_to_end\n");
    namespace ot = oigtl::transport;

    auto [a, b] = ot::make_loopback_pair();

    std::array<std::uint8_t, oigtl::runtime::kHeaderSize> hdr{};
    std::vector<std::uint8_t> body = {9, 8, 7};
    oigtl::runtime::pack_header(hdr, 1, "STATUS", "d", 0,
                                body.data(), body.size());
    std::vector<std::uint8_t> wire;
    wire.insert(wire.end(), hdr.begin(), hdr.end());
    wire.insert(wire.end(), body.begin(), body.end());

    ot::sync::block_on(a->send(wire), std::chrono::milliseconds(50));
    auto inc = ot::sync::block_on(b->receive(),
                                  std::chrono::milliseconds(50));
    REQUIRE(inc.body == body);
}

void test_void_future_form() {
    std::fprintf(stderr, "test_void_future_form\n");
    namespace ot = oigtl::transport;
    ot::Promise<void> p;
    auto fut = p.get_future();
    std::thread t([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        p.set_value();
    });
    ot::sync::block_on(std::move(fut), std::chrono::milliseconds(100));
    t.join();
}

void test_concurrent_block_on() {
    std::fprintf(stderr, "test_concurrent_block_on\n");
    namespace ot = oigtl::transport;

    ot::Promise<int> p1, p2;
    auto f1 = p1.get_future();
    auto f2 = p2.get_future();

    std::atomic<int> got1{0}, got2{0};
    std::thread t1([&] {
        got1 = ot::sync::block_on(std::move(f1),
                                  std::chrono::seconds(1));
    });
    std::thread t2([&] {
        got2 = ot::sync::block_on(std::move(f2),
                                  std::chrono::seconds(1));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    p1.set_value(111);
    p2.set_value(222);

    t1.join();
    t2.join();
    REQUIRE(got1.load() == 111);
    REQUIRE(got2.load() == 222);
}

}  // namespace

int main() {
    test_ready_returns_value();
    test_timeout();
    test_exception_propagates();
    test_loopback_end_to_end();
    test_void_future_form();
    test_concurrent_block_on();

    if (g_fail_count == 0) {
        std::fprintf(stderr, "transport_sync_test: all passed\n");
        return 0;
    }
    std::fprintf(stderr, "transport_sync_test: %d failure(s)\n",
                 g_fail_count);
    return 1;
}
