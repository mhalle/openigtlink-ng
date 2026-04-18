// Phase 4.5 acceptance: exercise the lovely API.
//
// Builds a Client and a Server over real TCP and pushes typed
// messages through the full stack:
//   - Envelope<T> round-trip
//   - v1 body-only path + v2 with metadata path
//   - Client::send(typed body)
//   - Client::receive<T>()
//   - Client::request_response<Reply>(Request)
//   - Server::on<T>(...).run()
//   - Timestamp + metadata helpers

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "oigtl/client.hpp"
#include "oigtl/envelope.hpp"
#include "oigtl/messages/status.hpp"
#include "oigtl/messages/transform.hpp"
#include "oigtl/pack.hpp"
#include "oigtl/server.hpp"

namespace om = oigtl::messages;

namespace {

int g_fail_count = 0;

#define REQUIRE(expr) do {                                            \
    if (!(expr)) {                                                    \
        std::fprintf(stderr, "  FAIL: %s:%d  %s\n",                   \
                     __FILE__, __LINE__, #expr);                      \
        ++g_fail_count;                                               \
    }                                                                 \
} while (0)

void test_timestamp_roundtrip() {
    std::fprintf(stderr, "test_timestamp_roundtrip\n");
    // 2024-06-15 12:34:56.789 UTC chosen arbitrarily.
    auto tp = std::chrono::system_clock::from_time_t(1718455896)
              + std::chrono::milliseconds(789);
    auto ts = oigtl::to_igtl_timestamp(tp);
    auto back = oigtl::from_igtl_timestamp(ts);
    auto err = std::chrono::duration_cast<std::chrono::microseconds>(
        back - tp).count();
    // Conversion through 32-bit fraction loses < 1 µs.
    REQUIRE(err >= -1 && err <= 1);
}

void test_metadata_helpers() {
    std::fprintf(stderr, "test_metadata_helpers\n");
    auto e1 = oigtl::make_text_metadata("PatientID", "P-001");
    REQUIRE(e1.key == "PatientID");
    REQUIRE(e1.value_encoding == 3);
    auto e2 = oigtl::make_utf8_metadata("Operator", "Dr. Ørsted");
    REQUIRE(e2.value_encoding == 106);
    oigtl::Metadata m{e1, e2};
    auto v = oigtl::metadata_text(m, "PatientID");
    REQUIRE(v.has_value() && *v == "P-001");
    REQUIRE(!oigtl::metadata_text(m, "Missing").has_value());
}

void test_pack_unpack_v1() {
    std::fprintf(stderr, "test_pack_unpack_v1\n");
    oigtl::Envelope<om::Transform> env;
    env.version = 1;
    env.device_name = "Tracker_A";
    env.timestamp = oigtl::now_igtl();
    env.body.matrix = {1, 0, 0, 10,
                       0, 1, 0, 20,
                       0, 0, 1, 30};

    auto wire = oigtl::pack(env);

    // Synthesize an Incoming as if the wire was just received.
    oigtl::transport::Incoming inc;
    inc.header = oigtl::runtime::unpack_header(wire.data(), wire.size());
    inc.body.assign(wire.begin() + oigtl::runtime::kHeaderSize,
                    wire.end());

    auto env2 = oigtl::unpack<om::Transform>(inc);
    REQUIRE(env2.version == 1);
    REQUIRE(env2.device_name == "Tracker_A");
    REQUIRE(env2.body.matrix[3] == 10.0f);
    REQUIRE(env2.body.matrix[7] == 20.0f);
    REQUIRE(env2.body.matrix[11] == 30.0f);
}

void test_pack_unpack_v2_with_metadata() {
    std::fprintf(stderr, "test_pack_unpack_v2_with_metadata\n");
    oigtl::Envelope<om::Transform> env;
    env.version = 2;
    env.device_name = "Tracker_B";
    env.timestamp = oigtl::now_igtl();
    env.message_id = 42;
    env.body.matrix = {1, 0, 0, 0,
                       0, 1, 0, 0,
                       0, 0, 1, 0};
    env.metadata.push_back(
        oigtl::make_text_metadata("PatientID", "P-001"));
    env.metadata.push_back(
        oigtl::make_text_metadata("Modality", "MR"));

    auto wire = oigtl::pack(env);

    oigtl::transport::Incoming inc;
    inc.header = oigtl::runtime::unpack_header(wire.data(), wire.size());
    inc.body.assign(wire.begin() + oigtl::runtime::kHeaderSize,
                    wire.end());

    auto env2 = oigtl::unpack<om::Transform>(inc);
    REQUIRE(env2.version == 2);
    REQUIRE(env2.message_id == 42);
    REQUIRE(env2.metadata.size() == 2);
    auto pid = oigtl::metadata_text(env2.metadata, "PatientID");
    REQUIRE(pid.has_value() && *pid == "P-001");
    auto mod = oigtl::metadata_text(env2.metadata, "Modality");
    REQUIRE(mod.has_value() && *mod == "MR");
}

void test_type_mismatch_throws() {
    std::fprintf(stderr, "test_type_mismatch_throws\n");
    oigtl::Envelope<om::Transform> env;
    env.version = 1;
    env.device_name = "X";
    env.body.matrix = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    auto wire = oigtl::pack(env);
    oigtl::transport::Incoming inc;
    inc.header = oigtl::runtime::unpack_header(wire.data(), wire.size());
    inc.body.assign(wire.begin() + oigtl::runtime::kHeaderSize,
                    wire.end());
    bool caught = false;
    try { (void)oigtl::unpack<om::Status>(inc); }
    catch (const oigtl::MessageTypeMismatch& e) {
        caught = (e.expected() == "STATUS") &&
                 (e.got() == "TRANSFORM");
    }
    REQUIRE(caught);
}

// Run a Server on an ephemeral port. Returns the port.
std::uint16_t start_echo_server(std::atomic<bool>& stop_flag,
                                std::thread& server_thread) {
    auto server_ptr = std::make_shared<oigtl::Server>(
        oigtl::Server::listen(0));
    auto port = server_ptr->local_port();

    server_ptr->on<om::Transform>(
        [](oigtl::Client& peer,
           const oigtl::Envelope<om::Transform>& env) {
            // Echo the transform back with our own device name.
            oigtl::Envelope<om::Transform> reply;
            reply.version = env.version;
            reply.device_name = "echo-server";
            reply.body = env.body;
            reply.metadata = env.metadata;
            peer.send(reply);
        });
    server_ptr->on<om::Status>(
        [](oigtl::Client& peer,
           const oigtl::Envelope<om::Status>& env) {
            // On status request, respond with a pong status.
            om::Status pong;
            pong.code = 1;
            pong.subcode = 0;
            pong.error_name = "OK";
            pong.status_message = "pong: " + env.body.status_message;
            peer.send("echo-server", pong);
        });

    server_thread = std::thread([server_ptr, &stop_flag]() {
        std::thread runner([server_ptr] { server_ptr->run(); });
        while (!stop_flag.load()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(20));
        }
        server_ptr->stop();
        if (runner.joinable()) runner.join();
    });

    // Give the server a beat to start accepting.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return port;
}

void test_client_send_receive() {
    std::fprintf(stderr, "test_client_send_receive\n");
    std::atomic<bool> stop{false};
    std::thread server_thread;
    auto port = start_echo_server(stop, server_thread);

    auto client = oigtl::Client::connect("127.0.0.1", port);

    // Send a transform; expect it echoed back.
    om::Transform xform;
    xform.matrix = {1,0,0, 1.5f,
                    0,1,0, 2.5f,
                    0,0,1, 3.5f};
    client.send("test-client", xform);

    auto reply = client.receive<om::Transform>();
    REQUIRE(reply.body.matrix[3] == 1.5f);
    REQUIRE(reply.body.matrix[7] == 2.5f);
    REQUIRE(reply.body.matrix[11] == 3.5f);
    REQUIRE(reply.device_name == "echo-server");

    client.close();
    stop = true;
    if (server_thread.joinable()) server_thread.join();
}

void test_request_response() {
    std::fprintf(stderr, "test_request_response\n");
    std::atomic<bool> stop{false};
    std::thread server_thread;
    auto port = start_echo_server(stop, server_thread);

    auto client = oigtl::Client::connect("127.0.0.1", port);

    om::Status req;
    req.code = 1;
    req.subcode = 0;
    req.error_name = "ping";
    req.status_message = "hello";

    auto reply = client.request_response<om::Status>(
        req, std::chrono::seconds(2));
    REQUIRE(reply.body.code == 1);
    REQUIRE(reply.body.status_message == "pong: hello");
    REQUIRE(reply.device_name == "echo-server");

    client.close();
    stop = true;
    if (server_thread.joinable()) server_thread.join();
}

void test_dispatch_loop() {
    std::fprintf(stderr, "test_dispatch_loop\n");
    std::atomic<bool> stop{false};
    std::thread server_thread;
    auto port = start_echo_server(stop, server_thread);

    auto client = oigtl::Client::connect("127.0.0.1", port);

    std::atomic<int> xform_count{0};
    std::atomic<int> status_count{0};

    client.on<om::Transform>(
        [&](const oigtl::Envelope<om::Transform>& env) {
            (void)env;
            ++xform_count;
        });
    client.on<om::Status>(
        [&](const oigtl::Envelope<om::Status>& env) {
            (void)env;
            ++status_count;
            if (status_count >= 3) {
                // Got enough; wind down.
            }
        });

    // Drive 3 transforms and 3 statuses through the echo server.
    for (int i = 0; i < 3; ++i) {
        om::Transform x;
        x.matrix = {1,0,0,float(i),  0,1,0,0,  0,0,1,0};
        client.send(x);
    }
    for (int i = 0; i < 3; ++i) {
        om::Status s;
        s.code = 1;
        s.error_name = "t";
        s.status_message = "n";
        client.send(s);
    }

    // Run dispatch in this thread; stop after a short timeout.
    std::thread stopper([&] {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(300));
        client.stop();
    });
    client.run();
    stopper.join();

    REQUIRE(xform_count.load() == 3);
    REQUIRE(status_count.load() == 3);

    stop = true;
    if (server_thread.joinable()) server_thread.join();
}

}  // namespace

void test_server_restrict_loopback_accepts() {
    std::fprintf(stderr, "test_server_restrict_loopback_accepts\n");
    // A peer on 127.0.0.1 must be accepted by
    // restrict_to_this_machine_only() (loopback is in the
    // allow-list by definition).
    auto server = oigtl::Server::listen(0);
    server.restrict_to_this_machine_only();
    const auto port = server.local_port();

    std::atomic<bool> got_peer{false};
    std::thread acc_th([&] {
        try {
            auto peer = server.accept();
            got_peer.store(true);
        } catch (...) { /* ok — restrict-then-kill races */ }
    });

    // Give the acceptor a moment to register.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto client = oigtl::Client::connect("127.0.0.1", port);
    // Wait a bounded time for the accept to complete.
    for (int i = 0; i < 40 && !got_peer.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    REQUIRE(got_peer.load());

    server.stop();
    if (acc_th.joinable()) acc_th.join();
}

void test_server_allow_peer_range_refuses_outside() {
    std::fprintf(stderr,
                 "test_server_allow_peer_range_refuses_outside\n");
    // Allow-list a range that excludes 127.0.0.1. A loopback
    // client should be refused — accept_loop closes the socket
    // without delivering the peer to the server.
    auto server = oigtl::Server::listen(0);
    server.allow_peer_range("10.0.0.1", "10.0.0.5");
    const auto port = server.local_port();

    std::atomic<bool> got_peer{false};
    std::thread acc_th([&] {
        try {
            auto peer = server.accept();
            got_peer.store(true);
        } catch (...) { /* expected: accept bails or reschedules */ }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    bool connect_threw = false;
    try {
        auto client = oigtl::Client::connect("127.0.0.1", port);
        // Even if TCP handshake succeeds, any attempt at read/send
        // should fail because the acceptor closes the connection
        // immediately on policy rejection. Prod the channel to
        // force a failure we can observe.
        std::this_thread::sleep_for(
            std::chrono::milliseconds(100));
        oigtl::messages::Status s;
        s.code = 1;
        client.send(s);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(50));
    } catch (...) {
        connect_threw = true;
    }
    // The peer must NOT have been delivered to the server, even
    // if the handshake came far enough for the client's connect
    // to return.
    REQUIRE(!got_peer.load());
    (void)connect_threw;  // connect itself may or may not throw.

    server.stop();
    if (acc_th.joinable()) acc_th.join();
}

int main() {
    test_timestamp_roundtrip();
    test_metadata_helpers();
    test_pack_unpack_v1();
    test_pack_unpack_v2_with_metadata();
    test_type_mismatch_throws();
    test_client_send_receive();
    test_request_response();
    test_dispatch_loop();
    test_server_restrict_loopback_accepts();
    test_server_allow_peer_range_refuses_outside();

    if (g_fail_count == 0) {
        std::fprintf(stderr, "ergo_test: all passed\n");
        return 0;
    }
    std::fprintf(stderr, "ergo_test: %d failure(s)\n", g_fail_count);
    return 1;
}
