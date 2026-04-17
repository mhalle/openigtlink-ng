// IO runtime singleton — one asio::io_context on one background
// thread, spun up on first access. Shutdown runs at static-dtor
// time (ok because no transport calls happen after main returns;
// users hold Connection unique_ptrs whose dtors run before).

#include "io_runtime.hpp"

#include <thread>

namespace oigtl::transport::detail {

namespace {

struct IoRuntime {
    asio::io_context ctx;
    // Keeps the io_context running even when there's no pending
    // work; work is posted over the lifetime of Connections which
    // live on the calling threads.
    asio::executor_work_guard<asio::io_context::executor_type> guard{
        asio::make_work_guard(ctx)};
    std::thread runner{[this] { ctx.run(); }};

    ~IoRuntime() {
        guard.reset();  // allow io_context::run to return
        ctx.stop();     // cancel any still-pending ops
        if (runner.joinable()) runner.join();
    }
};

}  // namespace

asio::io_context& io_ctx() {
    // Meyers singleton: thread-safe init under C++11 and later.
    static IoRuntime rt;
    return rt.ctx;
}

}  // namespace oigtl::transport::detail
