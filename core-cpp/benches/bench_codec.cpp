// Micro-benchmark for the C++ wire codec.
//
// Reports µs/op and MB/s for pack and unpack across a representative
// spread of fixtures: TRANSFORM (48-byte fixed body, hot path),
// IMAGE (~2.5KB, primitive-only with a remaining-counted pixel
// vector), VIDEOMETA (struct-element array). Plus CRC-64
// throughput on the IMAGE body.
//
// Each function is calibrated to run for ~1 second of wall time
// after a 10-iteration warmup. We use a single std::chrono::steady
// timer rather than pulling in a dep like benchmark/, since this
// binary's purpose is sanity-checking, not regression-tracking.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

extern "C" {
#include "igtl_test_data_image.h"
#include "igtl_test_data_transform.h"
#include "igtl_test_data_videometa.h"
}

#include "oigtl/messages/image.hpp"
#include "oigtl/messages/register_all.hpp"
#include "oigtl/messages/transform.hpp"
#include "oigtl/messages/videometa.hpp"
#include "oigtl/runtime/crc64.hpp"
#include "oigtl/runtime/header.hpp"
#include "oigtl/runtime/oracle.hpp"

namespace {

double bench_seconds(const std::function<void()>& fn,
                     double seconds = 1.0,
                     std::size_t min_iters = 1000) {
    // Warmup so cache + branch predictor are in a steady state
    // before we start the clock.
    for (int i = 0; i < 10; ++i) fn();

    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();
    const auto deadline = t0 + std::chrono::duration<double>(seconds);
    std::size_t iters = 0;
    while (true) {
        fn();
        ++iters;
        if (iters >= min_iters && Clock::now() >= deadline) break;
    }
    const auto elapsed = std::chrono::duration<double>(
        Clock::now() - t0).count();
    return (elapsed / static_cast<double>(iters)) * 1e6;  // µs/op
}

void print_row(const char* label, double us_per_op, std::size_t bytes) {
    const double mb_per_s = (bytes / (us_per_op * 1e-6)) / (1024.0 * 1024.0);
    std::printf("  %-22s %10.3f µs/op   %10.1f MB/s\n",
                label, us_per_op, mb_per_s);
}

template <typename Message>
void bench_message(const char* name,
                   const std::uint8_t* wire,
                   std::size_t wire_len) {
    std::printf("\n%s  (%zu wire bytes)\n", name, wire_len);
    std::printf("  %-22s %10s        %10s\n",
                "operation", "time", "throughput");
    std::printf("  %s\n",
                "----------------------------------------"
                "-------------------------");

    auto framing = oigtl::runtime::oracle::parse_wire(wire, wire_len);
    if (!framing.ok) {
        std::fprintf(stderr, "  oracle parse failed: %s\n",
                     framing.error.c_str());
        return;
    }
    const auto& body = framing.content_bytes;
    Message msg = Message::unpack(body.data(), body.size());

    // Hold the result somewhere the optimizer can't constant-fold
    // it away. Volatile sink; cheap.
    volatile std::size_t sink = 0;

    print_row("unpack body",
              bench_seconds([&] {
                  Message m = Message::unpack(body.data(), body.size());
                  sink ^= m.pack().size();  // force keep
              }),
              body.size());

    print_row("pack body",
              bench_seconds([&] {
                  auto buf = msg.pack();
                  sink ^= buf.size();
              }),
              body.size());

    // Full message round-trip via the oracle (parse + framing +
    // dispatch + reassembly + byte compare).
    auto registry = oigtl::messages::default_registry();
    print_row("oracle round-trip",
              bench_seconds([&] {
                  auto r = oigtl::runtime::oracle::verify_wire_bytes(
                      wire, wire_len, registry);
                  sink ^= static_cast<std::size_t>(r.ok);
              }),
              wire_len);

    (void)sink;
}

void bench_crc64(const std::uint8_t* data, std::size_t length) {
    std::printf("\nCRC-64 ECMA-182  (%zu bytes)\n", length);
    std::printf("  %s\n",
                "----------------------------------------"
                "-------------------------");
    volatile std::uint64_t sink = 0;
    print_row("crc64",
              bench_seconds([&] {
                  sink ^= oigtl::runtime::crc64(data, length);
              }),
              length);
    (void)sink;
}

void bench_header() {
    std::printf("\nHeader  (58 bytes)\n");
    std::printf("  %s\n",
                "----------------------------------------"
                "-------------------------");
    const std::uint8_t* data =
        reinterpret_cast<const std::uint8_t*>(test_transform_message);

    volatile std::size_t sink = 0;
    print_row("unpack_header",
              bench_seconds([&] {
                  auto h = oigtl::runtime::unpack_header(
                      data, oigtl::runtime::kHeaderSize);
                  sink ^= h.body_size;
              }),
              oigtl::runtime::kHeaderSize);

    std::array<std::uint8_t, oigtl::runtime::kHeaderSize> out{};
    const std::uint8_t* body =
        data + oigtl::runtime::kHeaderSize;
    const std::size_t body_len =
        sizeof(test_transform_message) - oigtl::runtime::kHeaderSize;
    print_row("pack_header (incl CRC)",
              bench_seconds([&] {
                  oigtl::runtime::pack_header(
                      out, 1, "TRANSFORM", "DeviceName",
                      0x49960000ULL, body, body_len);
                  sink ^= out[0];
              }),
              oigtl::runtime::kHeaderSize + body_len);

    (void)sink;
}

}  // namespace

int main() {
    std::printf("oigtl C++ codec — microbenchmark\n");
    std::printf("================================\n");

    bench_header();
    bench_message<oigtl::messages::Transform>(
        "TRANSFORM",
        reinterpret_cast<const std::uint8_t*>(test_transform_message),
        sizeof(test_transform_message));
    bench_message<oigtl::messages::Image>(
        "IMAGE",
        reinterpret_cast<const std::uint8_t*>(test_image_message),
        sizeof(test_image_message));
    bench_message<oigtl::messages::Videometa>(
        "VIDEOMETA (v3, struct array)",
        reinterpret_cast<const std::uint8_t*>(test_videometa_message),
        sizeof(test_videometa_message));

    // CRC throughput on the IMAGE body — the largest body that
    // matters for measuring per-byte work.
    auto framing = oigtl::runtime::oracle::parse_wire(
        reinterpret_cast<const std::uint8_t*>(test_image_message),
        sizeof(test_image_message));
    if (framing.ok) {
        bench_crc64(framing.content_bytes.data(),
                    framing.content_bytes.size());
    }

    return 0;
}
