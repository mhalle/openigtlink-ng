// Regression test: pack_metadata must reject inputs whose size
// exceeds what the wire format can represent. The previous
// implementation silently truncated counts and sizes with static_cast
// (u16 for count + key size, u32 for value size) while still copying
// the *untruncated* key/value payloads — a classic heap overflow on
// the pre-sized body buffer.
//
// After the fix, each oversize case raises MalformedMessageError.

#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "oigtl/runtime/error.hpp"
#include "oigtl/runtime/metadata.hpp"

namespace {

int g_fail_count = 0;

#define REQUIRE(expr) do {                                            \
    if (!(expr)) {                                                    \
        std::fprintf(stderr, "  FAIL: %s:%d  %s\n",                   \
                     __FILE__, __LINE__, #expr);                      \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

void test_rejects_too_many_entries() {
    std::fprintf(stderr, "test_rejects_too_many_entries\n");

    // 65536 entries — one past u16 count capacity. Keep each entry
    // tiny so the test doesn't burn memory.
    std::vector<oigtl::runtime::MetadataEntry> entries(
        std::size_t(std::numeric_limits<std::uint16_t>::max()) + 1);
    for (auto& e : entries) {
        e.key = "k";
        e.value_encoding = 3;  // ASCII
        e.value = {'v'};
    }

    bool caught = false;
    try {
        (void)oigtl::runtime::pack_metadata(entries);
    } catch (const oigtl::error::MalformedMessageError&) {
        caught = true;
    }
    REQUIRE(caught);
}

void test_rejects_oversize_key() {
    std::fprintf(stderr, "test_rejects_oversize_key\n");

    oigtl::runtime::MetadataEntry e;
    // 65536 bytes — one past u16 key-size capacity.
    e.key.assign(std::size_t(std::numeric_limits<std::uint16_t>::max()) + 1,
                 'x');
    e.value_encoding = 3;
    e.value = {'v'};

    bool caught = false;
    try {
        (void)oigtl::runtime::pack_metadata({e});
    } catch (const oigtl::error::MalformedMessageError&) {
        caught = true;
    }
    REQUIRE(caught);
}

// Note: a value-size-exceeds-u32 test would require allocating
// >4 GiB, which makes the test effectively impossible to run on CI
// machines. The guard for it is identical in shape to the key-size
// and count guards above, all three go through the same validation
// block, and the unit test exercises two of the three branches. We
// opt to document rather than allocate 4 GiB in a unit test.

void test_accepts_boundary_sizes() {
    std::fprintf(stderr, "test_accepts_boundary_sizes\n");

    oigtl::runtime::MetadataEntry e;
    // Exactly at u16 key-size capacity — must still pack.
    e.key.assign(std::numeric_limits<std::uint16_t>::max(), 'x');
    e.value_encoding = 3;
    e.value = {'v'};

    bool ok = true;
    try {
        (void)oigtl::runtime::pack_metadata({e});
    } catch (...) {
        ok = false;
    }
    REQUIRE(ok);
}

}  // namespace

int main() {
    test_rejects_too_many_entries();
    test_rejects_oversize_key();
    test_accepts_boundary_sizes();

    if (g_fail_count == 0) {
        std::fprintf(stderr, "metadata_overflow_test: all passed\n");
        return 0;
    }
    std::fprintf(stderr, "metadata_overflow_test: %d failure(s)\n",
                 g_fail_count);
    return 1;
}
