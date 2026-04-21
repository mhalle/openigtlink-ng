// resilient_client.cpp — demonstrates oigtl::Client's resilience
// features end-to-end.
//
// Runs client + server in one process so the demo is
// self-contained. The server is deliberately dropped mid-session
// to force the client's auto_reconnect path, then restarted on
// the same port so the buffered messages drain.
//
// Build:
//   cmake --build core-cpp/build --target resilient_client
//   ./core-cpp/build/resilient_client
//
// Concept doc: core-cpp/CLIENT_GUIDE.md.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>

#include "oigtl/client.hpp"
#include "oigtl/messages/status.hpp"
#include "oigtl/messages/transform.hpp"
#include "oigtl/server.hpp"

namespace msg = oigtl::messages;

// ----------------------------------------------------------------------
// ServerSession — owns an accepted peer and a reader thread. The
// reader exits promptly when the peer is closed from the outside
// (via stop()) or when the client disconnects naturally. Lets
// main() drop the server-side view of the connection without
// waiting for TCP to notice a vanished listener.
// ----------------------------------------------------------------------
class ServerSession {
 public:
    ServerSession(std::uint16_t port, std::atomic<int>& counter)
        : server_(oigtl::Server::listen(port, make_opt())),
          port_(server_.local_port()),
          counter_(counter) {
        reader_ = std::thread([this] { run(); });
    }

    ~ServerSession() {
        stop();
        if (reader_.joinable()) reader_.join();
    }

    std::uint16_t port() const { return port_; }

    void stop() {
        stop_requested_.store(true);
        // Close the server's accept socket (prevents future
        // accepts and breaks the current one, if any).
        server_.close();
        // Close the current peer so its receive_any unblocks.
        std::lock_guard<std::mutex> lk(peer_mu_);
        if (peer_) { try { peer_->close(); } catch (...) {} }
    }

 private:
    static oigtl::ServerOptions make_opt() {
        oigtl::ServerOptions o;
        // Short receive_timeout on the accepted peer so its
        // receive_any loop ticks and can notice stop().
        o.client_defaults.receive_timeout =
            std::chrono::milliseconds(200);
        return o;
    }

    void run() {
        try {
            auto p = server_.accept();
            {
                std::lock_guard<std::mutex> lk(peer_mu_);
                peer_ = std::make_unique<oigtl::Client>(std::move(p));
            }
            while (!stop_requested_.load()) {
                try {
                    auto inc = peer_->receive_any();
                    ++counter_;
                    if (inc.header.type_id == "TRANSFORM") {
                        auto env = oigtl::unpack_envelope<msg::Transform>(inc);
                        std::printf(
                            "  [server] TRANSFORM from %s: "
                            "[%.2f, %.2f, %.2f]\n",
                            inc.header.device_name.c_str(),
                            env.body.matrix[9],    // tx
                            env.body.matrix[10],   // ty
                            env.body.matrix[11]);  // tz
                    }
                } catch (const oigtl::transport::TimeoutError&) {
                    continue;    // tick and re-check stop_requested
                }
            }
        } catch (const std::exception& e) {
            std::printf("  [server] session ended: %s\n", e.what());
        }
    }

    oigtl::Server server_;
    std::uint16_t port_;
    std::atomic<int>& counter_;
    std::thread reader_;
    std::atomic<bool> stop_requested_{false};
    std::mutex peer_mu_;
    std::unique_ptr<oigtl::Client> peer_;
};

int main() {
    std::atomic<int> rx1{0};
    std::atomic<int> rx2{0};

    // Phase 1 server on a random free port. `std::uint16_t{0}`
    // rather than bare 0 so MSVC's /W4 doesn't warn about
    // narrowing the int literal through make_unique's perfect
    // forwarding.
    auto s1 = std::make_unique<ServerSession>(std::uint16_t{0}, rx1);
    const auto port = s1->port();

    oigtl::ClientOptions opt;
    opt.auto_reconnect          = true;
    opt.tcp_keepalive           = true;
    opt.offline_buffer_capacity = 20;
    opt.offline_overflow_policy =
        oigtl::ClientOptions::OfflineOverflow::DropOldest;
    opt.reconnect_initial_backoff = std::chrono::milliseconds(100);
    opt.reconnect_max_backoff     = std::chrono::milliseconds(500);
    opt.receive_timeout           = std::chrono::milliseconds(200);
    opt.default_device            = "demo-tracker";

    std::printf("[client] connecting to 127.0.0.1:%u ...\n", port);
    auto client = oigtl::Client::connect("127.0.0.1", port, opt);

    std::atomic<int> reconnects{0};
    std::atomic<int> drops{0};
    client.on_connected([&] {
        std::printf("[client] on_connected (#%d)\n", ++reconnects);
    });
    client.on_disconnected([&](std::exception_ptr) {
        std::printf("[client] on_disconnected\n");
        ++drops;
    });
    client.on_reconnect_failed(
        [&](int attempt, std::chrono::milliseconds delay) {
            std::printf(
                "[client] reconnect attempt %d failed; "
                "next in %lld ms\n",
                attempt,
                static_cast<long long>(delay.count()));
        });

    auto send_tf = [&](float tx, float ty, float tz) {
        msg::Transform m;
        // Column-major 3x4: R11..R33 then TX, TY, TZ.
        m.matrix = { 1,0,0, 0,1,0, 0,0,1, tx, ty, tz };
        client.send(m);
    };

    // ---- Phase 1: happy path --------------------------------------
    std::printf("[client] Phase 1: 3 messages while connected\n");
    for (int i = 0; i < 3; ++i) {
        send_tf(float(i), float(i * 2), float(i * 3));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // ---- Phase 2: induce drop -------------------------------------
    std::printf("[client] Phase 2: stopping server to induce drop\n");
    s1.reset();    // joins the reader thread; drops the peer

    std::printf("[client] Phase 2: 5 messages during outage\n");
    for (int i = 3; i < 8; ++i) {
        try {
            send_tf(float(i), float(i * 2), float(i * 3));
        } catch (const std::exception& e) {
            std::printf("[client] send %d threw: %s\n", i, e.what());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // ---- Phase 3: restart server ----------------------------------
    std::printf("[client] Phase 3: restart server on port %u\n", port);
    auto s2 = std::make_unique<ServerSession>(port, rx2);
    std::this_thread::sleep_for(std::chrono::seconds(1));   // reconnect

    std::printf("[client] Phase 3: 2 post-reconnect messages\n");
    for (int i = 8; i < 10; ++i) {
        send_tf(float(i), float(i * 2), float(i * 3));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // ---- Cleanup --------------------------------------------------
    client.close();
    s2.reset();

    std::printf("\nSUMMARY:\n");
    std::printf("  server session 1 received: %d messages\n",
                rx1.load());
    std::printf("  server session 2 received: %d messages\n",
                rx2.load());
    std::printf("  disconnects observed:      %d\n", drops.load());
    std::printf("  reconnects observed:       %d\n", reconnects.load());

    const bool ok =
        rx1.load()        >= 3 &&
        rx2.load()        >= 2 &&
        drops.load()      >= 1 &&
        reconnects.load() >= 1;
    std::printf("\n%s\n", ok ? "PASS" : "FAIL (unexpected counts)");
    return ok ? 0 : 1;
}
