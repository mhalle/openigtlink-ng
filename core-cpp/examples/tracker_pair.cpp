// examples/tracker_pair.cpp — a complete, runnable OpenIGTLink
// program that exercises the ergonomic API end-to-end.
//
// Scenario: a "tracker" server emits TRANSFORM messages at 50 Hz
// for two seconds, carrying patient-context metadata. A
// "workstation" client connects, logs each pose, sends a STATUS
// acknowledgement every 20 messages, and exits cleanly on the
// tracker's STATUS goodbye.
//
// Both roles live in this single binary so it's trivial to run:
//     tracker_pair
//
// Build:
//     cmake --build build --target tracker_pair
// Run:
//     ./build/tracker_pair

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "oigtl/client.hpp"
#include "oigtl/envelope.hpp"
#include "oigtl/messages/status.hpp"
#include "oigtl/messages/transform.hpp"
#include "oigtl/server.hpp"

namespace om = oigtl::messages;

namespace {

constexpr std::uint16_t kPort = 18944;
constexpr int kPosesToEmit = 100;   // 100 @ 50 Hz = 2 s

// ---------------------------------------------------------------
// Tracker (server side)
// ---------------------------------------------------------------
void run_tracker() {
    oigtl::ServerOptions sopts;
    sopts.bind_address = "127.0.0.1";
    auto server = oigtl::Server::listen(kPort, sopts);

    std::printf("[tracker] listening on %u\n", server.local_port());

    // One-shot: accept a single workstation, stream poses, close.
    auto peer = server.accept();
    std::printf("[tracker] workstation connected from %s:%u\n",
                peer.connection().peer_address().c_str(),
                peer.connection().peer_port());

    // Also handle an incoming STATUS while we stream.
    std::atomic<int> acks{0};
    peer.on<om::Status>(
        [&](const oigtl::Envelope<om::Status>& env) {
            std::printf("[tracker] ACK #%d from %s: %s\n",
                        acks.load() + 1,
                        env.device_name.c_str(),
                        env.body.status_message.c_str());
            ++acks;
        });

    // Stream poses from a background thread; dispatch acks on main.
    std::atomic<bool> sending{true};
    std::thread streamer([&] {
        const auto period = std::chrono::milliseconds(20);
        for (int i = 0; i < kPosesToEmit; ++i) {
            // A probe slowly sliding along +Z.
            const float tz = static_cast<float>(i) * 0.5f;

            oigtl::Envelope<om::Transform> env;
            env.version = 2;
            env.device_name = "Probe_Tip";
            env.timestamp = oigtl::now_igtl();
            env.message_id = static_cast<std::uint32_t>(i);
            env.body.matrix = {
                1.0f, 0.0f, 0.0f,  0.0f,
                0.0f, 1.0f, 0.0f,  0.0f,
                0.0f, 0.0f, 1.0f,  tz,
            };
            env.metadata.push_back(
                oigtl::make_text_metadata("PatientID", "P-0421"));
            env.metadata.push_back(
                oigtl::make_text_metadata("SessionID", "S-2026-04-17-A"));
            env.metadata.push_back(
                oigtl::make_text_metadata("Frame",
                                          std::to_string(i)));

            try {
                peer.send(env);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "[tracker] send failed: %s\n",
                             e.what());
                break;
            }
            std::this_thread::sleep_for(period);
        }

        // Send STATUS=OK goodbye.
        om::Status bye;
        bye.code = 1;                  // 1 = OK
        bye.subcode = 0;
        bye.error_name = "OK";
        bye.status_message = "stream complete";
        try { peer.send("Probe_Tip", bye); }
        catch (...) { /* peer may have closed */ }
        sending = false;
    });

    std::thread dispatch([&] { peer.run(); });

    streamer.join();
    // Give the peer a moment to send any trailing acks.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    peer.stop();
    if (dispatch.joinable()) dispatch.join();
    server.stop();

    std::printf("[tracker] done; %d acks received\n", acks.load());
}

// ---------------------------------------------------------------
// Workstation (client side)
// ---------------------------------------------------------------
void run_workstation() {
    oigtl::ClientOptions copts;
    copts.connect_timeout = std::chrono::seconds(3);
    copts.connect_retries = 5;
    copts.retry_backoff   = std::chrono::milliseconds(100);
    copts.default_device  = "Workstation_A";
    auto client = oigtl::Client::connect("127.0.0.1", kPort, copts);
    std::printf("[workstation] connected to tracker\n");

    std::atomic<int> poses_received{0};
    std::atomic<bool> goodbye_seen{false};

    client
        .on<om::Transform>(
            [&](const oigtl::Envelope<om::Transform>& env) {
                const auto& m = env.body.matrix;
                const float tx = m[3], ty = m[7], tz = m[11];
                const auto patient =
                    oigtl::metadata_text(env.metadata, "PatientID")
                        .value_or("?");
                const auto frame =
                    oigtl::metadata_text(env.metadata, "Frame")
                        .value_or("?");

                std::printf(
                    "[workstation] #%s pose from %s "
                    "(patient=%s): t = (%.2f, %.2f, %.2f)\n",
                    frame.c_str(),
                    env.device_name.c_str(),
                    patient.c_str(),
                    tx, ty, tz);

                ++poses_received;

                // Acknowledge every 20th pose.
                if (poses_received.load() % 20 == 0) {
                    om::Status ack;
                    ack.code = 1;
                    ack.subcode = 0;
                    ack.error_name = "OK";
                    ack.status_message =
                        "received " +
                        std::to_string(poses_received.load()) +
                        " poses";
                    client.send(ack);
                }
            })
        .on<om::Status>(
            [&](const oigtl::Envelope<om::Status>& env) {
                std::printf(
                    "[workstation] STATUS from %s: "
                    "code=%u \"%s\"\n",
                    env.device_name.c_str(),
                    env.body.code,
                    env.body.status_message.c_str());
                goodbye_seen = true;
                client.stop();
            })
        .on_error(
            [&](std::exception_ptr ep) {
                try { std::rethrow_exception(ep); }
                catch (const oigtl::transport::ConnectionClosedError&) {
                    std::printf("[workstation] tracker closed\n");
                }
                catch (const std::exception& e) {
                    std::fprintf(stderr,
                        "[workstation] error: %s\n", e.what());
                }
            })
        .run();   // blocks until stop() or disconnect

    std::printf(
        "[workstation] done; %d poses received, goodbye=%s\n",
        poses_received.load(),
        goodbye_seen.load() ? "yes" : "no");
}

}  // namespace

int main() {
    std::thread tracker(run_tracker);
    // Stagger: let the tracker get into listen() before we connect.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    run_workstation();
    tracker.join();
    return 0;
}
