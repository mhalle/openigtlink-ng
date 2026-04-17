// Future<T> / Promise<T> — minimal async primitive for the transport.
//
// Design decision (TRANSPORT_PLAN.md §Decisions #1): we vendor our
// own rather than use std::future, because the transport needs
// cancellation, continuation (`then`), and composition (`when_all`)
// — none of which std::future provides.
//
// Scope (Phase 1):
//   - `Future<T>::get()`  blocking wait, returns T or throws
//   - `Future<T>::wait_for(duration)`  timed poll
//   - `Future<T>::cancel()`  request cancellation; if not yet
//                            fulfilled, resolves with
//                            OperationCancelledError
//   - `Future<T>::then(F)`  schedule F(T)→U; returns Future<U>
//   - `Promise<T>::set_value(T)` / `set_exception(eptr)`
//   - `Future<void>` specialization
//
// Out of scope (added as Phase 2+ needs them):
//   - `when_all` / `when_any`  — add when accept-loop needs them.
//   - Custom executors  — continuations currently run inline on the
//     fulfilling thread, which is fine for the loopback and for the
//     asio-thread-based TCP backend.
//
// Thread-safety: state is guarded by a mutex. Callbacks registered
// via `then()` run on whatever thread calls `set_value` /
// `set_exception` / `cancel` (inline). Cancellation is cooperative:
// the producer can inspect `cancel_requested()` on its shared state,
// but today nothing does — Phase 2's asio code will wire this.
#ifndef OIGTL_TRANSPORT_FUTURE_HPP
#define OIGTL_TRANSPORT_FUTURE_HPP

#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

#include "oigtl/transport/errors.hpp"

namespace oigtl::transport {

namespace detail {

// Shared state between a Promise<T> and its Future<T>. One instance
// per (Promise, Future) pair. Both sides hold a shared_ptr.
template <typename T>
struct FutureState {
    std::mutex mu;
    std::condition_variable cv;
    bool ready = false;
    bool cancel_requested = false;
    std::optional<T> value;
    std::exception_ptr error;
    // Invoked exactly once, under `mu`, when `ready` transitions
    // true. Set by Future::then(). Replaced with nullptr after
    // invocation so we never run it twice.
    std::function<void()> continuation;
};

// Void specialization: no `value` field.
struct FutureStateVoid {
    std::mutex mu;
    std::condition_variable cv;
    bool ready = false;
    bool cancel_requested = false;
    bool has_value = false;
    std::exception_ptr error;
    std::function<void()> continuation;
};

}  // namespace detail

template <typename T> class Promise;
template <typename T> class Future;

// ===========================================================================
// Future<T> — value path
// ===========================================================================
template <typename T>
class Future {
 public:
    Future() = default;  // "invalid" future (no state)
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;
    Future(Future&&) noexcept = default;
    Future& operator=(Future&&) noexcept = default;

    bool valid() const noexcept { return state_ != nullptr; }

    // Block until the future resolves. Returns the value on success;
    // rethrows the stored exception on failure (including
    // OperationCancelledError if cancelled).
    T get() {
        auto s = state_;
        std::unique_lock<std::mutex> lk(s->mu);
        s->cv.wait(lk, [&] { return s->ready; });
        if (s->error) std::rethrow_exception(s->error);
        return std::move(*s->value);
    }

    // Timed wait. Returns true if resolved within the deadline.
    template <class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& d) {
        auto s = state_;
        std::unique_lock<std::mutex> lk(s->mu);
        return s->cv.wait_for(lk, d, [&] { return s->ready; });
    }

    // Request cancellation. If already resolved, no-op. Otherwise
    // flips the state to "resolved with OperationCancelledError" so
    // any subsequent `get()` throws. Producers that honor
    // cancellation check `cancel_requested()` via their own
    // Promise-side helper (not exposed here; consumers of Future
    // don't need that view).
    void cancel() {
        auto s = state_;
        std::function<void()> cont;
        {
            std::lock_guard<std::mutex> lk(s->mu);
            if (s->ready) return;
            s->cancel_requested = true;
            s->error = std::make_exception_ptr(OperationCancelledError{});
            s->ready = true;
            cont = std::move(s->continuation);
        }
        s->cv.notify_all();
        if (cont) cont();
    }

    // Schedule `fn(T)` to run when this future resolves with a value.
    // Returns a new Future<U> where U = result of fn. If this future
    // resolves with an exception, the returned future resolves with
    // the same exception (fn is not invoked).
    template <class F>
    auto then(F&& fn) -> Future<std::invoke_result_t<F, T>>;

 private:
    friend class Promise<T>;
    explicit Future(std::shared_ptr<detail::FutureState<T>> s)
        : state_(std::move(s)) {}
    std::shared_ptr<detail::FutureState<T>> state_;
};

// ===========================================================================
// Future<void>
// ===========================================================================
template <>
class Future<void> {
 public:
    Future() = default;
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;
    Future(Future&&) noexcept = default;
    Future& operator=(Future&&) noexcept = default;

    bool valid() const noexcept { return state_ != nullptr; }

    void get() {
        auto s = state_;
        std::unique_lock<std::mutex> lk(s->mu);
        s->cv.wait(lk, [&] { return s->ready; });
        if (s->error) std::rethrow_exception(s->error);
    }

    template <class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& d) {
        auto s = state_;
        std::unique_lock<std::mutex> lk(s->mu);
        return s->cv.wait_for(lk, d, [&] { return s->ready; });
    }

    void cancel() {
        auto s = state_;
        std::function<void()> cont;
        {
            std::lock_guard<std::mutex> lk(s->mu);
            if (s->ready) return;
            s->cancel_requested = true;
            s->error = std::make_exception_ptr(OperationCancelledError{});
            s->ready = true;
            cont = std::move(s->continuation);
        }
        s->cv.notify_all();
        if (cont) cont();
    }

    template <class F>
    auto then(F&& fn) -> Future<std::invoke_result_t<F>>;

 private:
    friend class Promise<void>;
    explicit Future(std::shared_ptr<detail::FutureStateVoid> s)
        : state_(std::move(s)) {}
    std::shared_ptr<detail::FutureStateVoid> state_;
};

// ===========================================================================
// Promise<T>
// ===========================================================================
template <typename T>
class Promise {
 public:
    Promise() : state_(std::make_shared<detail::FutureState<T>>()) {}
    Promise(const Promise&) = delete;
    Promise& operator=(const Promise&) = delete;
    Promise(Promise&&) noexcept = default;
    Promise& operator=(Promise&&) noexcept = default;

    // Hand out the future. Call at most once — the state is shared
    // but the Future wrapper has move-only semantics.
    Future<T> get_future() { return Future<T>(state_); }

    void set_value(T v) {
        auto& s = state_;
        std::function<void()> cont;
        {
            std::lock_guard<std::mutex> lk(s->mu);
            if (s->ready) return;  // already cancelled or fulfilled
            s->value = std::move(v);
            s->ready = true;
            cont = std::move(s->continuation);
        }
        s->cv.notify_all();
        if (cont) cont();
    }

    void set_exception(std::exception_ptr e) {
        auto& s = state_;
        std::function<void()> cont;
        {
            std::lock_guard<std::mutex> lk(s->mu);
            if (s->ready) return;
            s->error = std::move(e);
            s->ready = true;
            cont = std::move(s->continuation);
        }
        s->cv.notify_all();
        if (cont) cont();
    }

    // Producer-side view of consumer-requested cancellation. A
    // long-running producer (e.g. an asio async_read) should poll
    // this and abort on true.
    bool cancel_requested() const {
        std::lock_guard<std::mutex> lk(state_->mu);
        return state_->cancel_requested;
    }

 private:
    std::shared_ptr<detail::FutureState<T>> state_;
};

// Promise<void>
template <>
class Promise<void> {
 public:
    Promise() : state_(std::make_shared<detail::FutureStateVoid>()) {}
    Promise(const Promise&) = delete;
    Promise& operator=(const Promise&) = delete;
    Promise(Promise&&) noexcept = default;
    Promise& operator=(Promise&&) noexcept = default;

    Future<void> get_future() { return Future<void>(state_); }

    void set_value() {
        auto& s = state_;
        std::function<void()> cont;
        {
            std::lock_guard<std::mutex> lk(s->mu);
            if (s->ready) return;
            s->has_value = true;
            s->ready = true;
            cont = std::move(s->continuation);
        }
        s->cv.notify_all();
        if (cont) cont();
    }

    void set_exception(std::exception_ptr e) {
        auto& s = state_;
        std::function<void()> cont;
        {
            std::lock_guard<std::mutex> lk(s->mu);
            if (s->ready) return;
            s->error = std::move(e);
            s->ready = true;
            cont = std::move(s->continuation);
        }
        s->cv.notify_all();
        if (cont) cont();
    }

    bool cancel_requested() const {
        std::lock_guard<std::mutex> lk(state_->mu);
        return state_->cancel_requested;
    }

 private:
    std::shared_ptr<detail::FutureStateVoid> state_;
};

// ===========================================================================
// then() implementations (after Promise is defined)
// ===========================================================================
namespace detail {

// Register a continuation under the state mutex. If already ready,
// run inline.
template <class State>
void attach_continuation(State& s, std::function<void()> cont) {
    bool run_now = false;
    {
        std::lock_guard<std::mutex> lk(s->mu);
        if (s->ready) {
            run_now = true;
        } else {
            s->continuation = std::move(cont);
        }
    }
    if (run_now) cont();  // note: caller moved, so use the name above
}

}  // namespace detail

template <typename T>
template <class F>
auto Future<T>::then(F&& fn) -> Future<std::invoke_result_t<F, T>> {
    using U = std::invoke_result_t<F, T>;
    auto upstream = state_;
    auto next = std::make_shared<Promise<U>>();
    auto fut = next->get_future();
    auto fn_holder = std::make_shared<std::decay_t<F>>(std::forward<F>(fn));

    auto cont = [upstream, next, fn_holder]() {
        // Read upstream result (no lock needed; `ready` has been set
        // and is not mutated again).
        if (upstream->error) {
            next->set_exception(upstream->error);
            return;
        }
        try {
            if constexpr (std::is_void_v<U>) {
                (*fn_holder)(std::move(*upstream->value));
                next->set_value();
            } else {
                next->set_value((*fn_holder)(std::move(*upstream->value)));
            }
        } catch (...) {
            next->set_exception(std::current_exception());
        }
    };

    // Inline register-or-run.
    bool run_now = false;
    {
        std::lock_guard<std::mutex> lk(upstream->mu);
        if (upstream->ready) run_now = true;
        else upstream->continuation = cont;
    }
    if (run_now) cont();
    return fut;
}

template <class F>
auto Future<void>::then(F&& fn) -> Future<std::invoke_result_t<F>> {
    using U = std::invoke_result_t<F>;
    auto upstream = state_;
    auto next = std::make_shared<Promise<U>>();
    auto fut = next->get_future();
    auto fn_holder = std::make_shared<std::decay_t<F>>(std::forward<F>(fn));

    auto cont = [upstream, next, fn_holder]() {
        if (upstream->error) {
            next->set_exception(upstream->error);
            return;
        }
        try {
            if constexpr (std::is_void_v<U>) {
                (*fn_holder)();
                next->set_value();
            } else {
                next->set_value((*fn_holder)());
            }
        } catch (...) {
            next->set_exception(std::current_exception());
        }
    };

    bool run_now = false;
    {
        std::lock_guard<std::mutex> lk(upstream->mu);
        if (upstream->ready) run_now = true;
        else upstream->continuation = cont;
    }
    if (run_now) cont();
    return fut;
}

// ===========================================================================
// Convenience helpers
// ===========================================================================
template <typename T>
Future<T> make_ready_future(T value) {
    Promise<T> p;
    auto f = p.get_future();
    p.set_value(std::move(value));
    return f;
}

inline Future<void> make_ready_future() {
    Promise<void> p;
    auto f = p.get_future();
    p.set_value();
    return f;
}

template <typename T>
Future<T> make_exceptional_future(std::exception_ptr e) {
    Promise<T> p;
    auto f = p.get_future();
    p.set_exception(std::move(e));
    return f;
}

}  // namespace oigtl::transport

#endif  // OIGTL_TRANSPORT_FUTURE_HPP
