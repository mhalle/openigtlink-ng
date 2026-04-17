// Sync bridge — let synchronous callers (the upstream-compat shim,
// CLI tools, simple scripts) drive the async Connection API.
//
// Header-only; no new .cpp. The library's I/O context (detail::
// io_ctx) is already pumped by its own thread, so any Future<T>
// will resolve without the caller needing to drive an executor.
// block_on just waits on the Future's condition variable with a
// deadline.
//
// Rationale (TRANSPORT_PLAN.md §Phase 4): the shim's upstream API
// is uniformly blocking (`sock->Send(msg)` returns 0/1). Rather
// than re-shape every shim call, we wrap its interior in
// `sync::block_on(fut, timeout)` and translate exceptions to
// upstream's int-return convention inside the shim itself.
#ifndef OIGTL_TRANSPORT_SYNC_HPP
#define OIGTL_TRANSPORT_SYNC_HPP

#include <chrono>
#include <utility>

#include "oigtl/transport/errors.hpp"
#include "oigtl/transport/future.hpp"

namespace oigtl::transport::sync {

// Block until `fut` resolves or `timeout` elapses.
//
// - Returns the Future's value on success.
// - Rethrows the Future's stored exception on failure (including
//   OperationCancelledError if the Future was cancelled).
// - Throws `TimeoutError` if the deadline elapses before the
//   Future resolves. The Future is NOT cancelled on timeout — the
//   caller chooses whether to retry or cancel.
//
// `fut` is taken by value (Future is move-only); the Future is
// consumed. This is deliberate — once you've blocked on a Future
// and either got the value or timed out, reusing the same Future
// is an anti-pattern.
template <typename T, class Rep, class Period>
T block_on(Future<T> fut, std::chrono::duration<Rep, Period> timeout) {
    if (!fut.wait_for(timeout)) {
        throw TimeoutError{};
    }
    return fut.get();  // returns void when T=void (legal)
}

// Overload: block indefinitely. The I/O context and any active
// Connection keep pending ops alive; if the op never resolves this
// will block forever, by the caller's choice.
template <typename T>
T block_on(Future<T> fut) {
    return fut.get();
}

}  // namespace oigtl::transport::sync

#endif  // OIGTL_TRANSPORT_SYNC_HPP
