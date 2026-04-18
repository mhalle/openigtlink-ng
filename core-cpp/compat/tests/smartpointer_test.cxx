// Phase 5 acceptance: refcount semantics match upstream.
//
// Tests the igtl::LightObject + igtl::SmartPointer contract that
// 3D Slicer, PLUS, and any upstream consumer depends on:
//   - New() returns a Pointer with refcount == 1
//   - Copy construct / copy assign of Pointer bumps refcount
//   - Pointer destruction decrements; hitting 0 deletes
//   - Raw-assign replaces target without transient drop to zero
//   - Thread-safe concurrent Register/UnRegister
//
// No GoogleTest — same REQUIRE style as the other tests.

#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

#include "igtl/igtlLightObject.h"
#include "igtl/igtlObject.h"
#include "igtl/igtlMacro.h"

namespace {

int g_fail_count = 0;

#define REQUIRE(expr) do {                                            \
    if (!(expr)) {                                                    \
        std::fprintf(stderr, "  FAIL: %s:%d  %s\n",                   \
                     __FILE__, __LINE__, #expr);                      \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

// Test subclass that bumps a static counter on destruction so we
// can observe when delete happens without peeking into m_ReferenceCount.
std::atomic<int> g_dtor_count{0};

class Probe : public igtl::LightObject {
 public:
    igtlTypeMacro(Probe, igtl::LightObject);
    igtlNewMacro(Probe);

    int payload = 42;

 protected:
    Probe() = default;
    ~Probe() override { g_dtor_count.fetch_add(1); }
};

void test_new_refcount_is_one() {
    std::fprintf(stderr, "test_new_refcount_is_one\n");
    const int before = g_dtor_count.load();
    {
        auto p = Probe::New();
        REQUIRE(p.IsNotNull());
        REQUIRE(p->GetReferenceCount() == 1);
        REQUIRE(p->payload == 42);
    }
    REQUIRE(g_dtor_count.load() == before + 1);
}

void test_copy_bumps_refcount() {
    std::fprintf(stderr, "test_copy_bumps_refcount\n");
    const int before = g_dtor_count.load();
    {
        auto p = Probe::New();
        {
            auto q = p;                     // copy construct
            REQUIRE(p->GetReferenceCount() == 2);
            REQUIRE(q->GetReferenceCount() == 2);
            auto r = p;
            REQUIRE(p->GetReferenceCount() == 3);
        }                                   // q, r go out of scope
        REQUIRE(p->GetReferenceCount() == 1);
    }
    REQUIRE(g_dtor_count.load() == before + 1);
}

void test_raw_assign_no_transient_zero() {
    std::fprintf(stderr, "test_raw_assign_no_transient_zero\n");
    const int before = g_dtor_count.load();
    {
        auto a = Probe::New();
        auto b = Probe::New();
        // Reassign a to point to b's pointee. After this, the old
        // a must be decremented exactly once and must NOT hit zero
        // transiently (it held refcount 1 before).
        a = b.GetPointer();
        REQUIRE(a.GetPointer() == b.GetPointer());
        REQUIRE(a->GetReferenceCount() == 2);
    }
    // Two Probes created, two destroyed.
    REQUIRE(g_dtor_count.load() == before + 2);
}

void test_self_assign_noop() {
    std::fprintf(stderr, "test_self_assign_noop\n");
    auto p = Probe::New();
    const int start = p->GetReferenceCount();
    p = p.GetPointer();  // raw-assign the same pointer
    REQUIRE(p->GetReferenceCount() == start);
}

void test_is_null_semantics() {
    std::fprintf(stderr, "test_is_null_semantics\n");
    igtl::SmartPointer<Probe> empty;
    REQUIRE(empty.IsNull());
    REQUIRE(!empty.IsNotNull());
    REQUIRE(empty.GetPointer() == nullptr);
}

void test_concurrent_register_unregister() {
    std::fprintf(stderr, "test_concurrent_register_unregister\n");
    auto p = Probe::New();
    // `constexpr` rather than `const int` so MSVC strict mode
    // treats it as a compile-time constant and doesn't demand
    // named capture inside the lambda below. Clang in turn would
    // warn (-Wunused-lambda-capture) if we captured it explicitly.
    constexpr int N = 1000;
    constexpr int threads = 8;

    std::vector<std::thread> ts;
    ts.reserve(threads);
    for (int i = 0; i < threads; ++i) {
        ts.emplace_back([&p]() {
            for (int j = 0; j < N; ++j) {
                p->Register();
                p->UnRegister();
            }
        });
    }
    for (auto& t : ts) t.join();

    REQUIRE(p->GetReferenceCount() == 1);
}

void test_delete_decrements() {
    std::fprintf(stderr, "test_delete_decrements\n");
    const int before = g_dtor_count.load();
    auto p = Probe::New();  // count=1
    p->Register();          // count=2
    p->Delete();            // count=1 — not yet destroyed
    REQUIRE(g_dtor_count.load() == before);
    REQUIRE(p->GetReferenceCount() == 1);
    // p goes out of scope → count=0 → destroyed.
}

void test_createanother() {
    std::fprintf(stderr, "test_createanother\n");
    const int before = g_dtor_count.load();
    {
        auto a = Probe::New();
        // Upstream's CreateAnother returns LightObject::Pointer
        // (not Probe::Pointer) so consumers cast back as needed.
        igtl::LightObject::Pointer b = a->CreateAnother();
        REQUIRE(b.IsNotNull());
        REQUIRE(b.GetPointer() != a.GetPointer());
        REQUIRE(b->GetReferenceCount() == 1);
    }
    REQUIRE(g_dtor_count.load() == before + 2);
}

}  // namespace

int main() {
    test_new_refcount_is_one();
    test_copy_bumps_refcount();
    test_raw_assign_no_transient_zero();
    test_self_assign_noop();
    test_is_null_semantics();
    test_concurrent_register_unregister();
    test_delete_decrements();
    test_createanother();

    if (g_fail_count == 0) {
        std::fprintf(stderr, "smartpointer_test: all passed\n");
        return 0;
    }
    std::fprintf(stderr, "smartpointer_test: %d failure(s)\n",
                 g_fail_count);
    return 1;
}
