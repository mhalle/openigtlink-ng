// Cross-runtime test fixture — a tiny C++ TCP server that mirrors
// `core-ts/tests/net/fixtures/python_ws_echo.py`.
//
// Protocol: bind a random free port on 127.0.0.1, print
// "PORT=<n>\n" (flushed) to stdout so the parent test can parse
// it, then accept exactly one peer and serve:
//
//   TRANSFORM → reply with STATUS carrying the last 3 matrix
//               values in status_message, in the format expected
//               by the existing python_ws_echo.py fixture:
//               "got matrix[-3:]=[X.Y, X.Y, X.Y]"
//
// After one round-trip, exit cleanly. The parent test owns timeouts
// and kills us on SIGTERM if we hang.
//
// The whole point of this fixture is proving wire parity between
// core-cpp's Server and foreign-language clients (core-py Client,
// core-ts WsClient wrapped-via-gateway, etc.) — the cross-language
// interop cell that cross_runtime.test.ts doesn't currently cover.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>

#include "oigtl/client.hpp"  // only for Envelope/Transform/Status types
#include "oigtl/envelope.hpp"
#include "oigtl/messages/status.hpp"
#include "oigtl/messages/transform.hpp"
#include "oigtl/server.hpp"

namespace om = oigtl::messages;

namespace {

// Format a float so the output matches Python's default "12.0"
// style (trailing .0 for integers). The Python fixture relies on
// this exact textual form; the parent test regex-matches it.
std::string fmt_float(float v) {
    // Python's repr for a float like 11.0 is "11.0". C++ default
    // printf %g prints "11". Force a ".0" suffix when the value
    // is integral.
    std::ostringstream os;
    const float r = static_cast<float>(static_cast<long long>(v));
    if (r == v) {
        os << static_cast<long long>(v) << ".0";
    } else {
        os.precision(8);
        os << v;
    }
    return os.str();
}

}  // namespace

int main() {
    oigtl::ServerOptions sopts;
    sopts.bind_address = "127.0.0.1";
    auto server = oigtl::Server::listen(0, sopts);  // 0 → OS picks

    // Emit the port on its own line + flush so the parent test
    // reads it immediately. Matches python_ws_echo.py.
    std::printf("PORT=%u\n", server.local_port());
    std::fflush(stdout);

    // Serve exactly one peer.
    auto peer = server.accept();

    // Block until a TRANSFORM arrives; the parent test's overall
    // timeout caps us if nothing ever shows up.
    auto env = peer.receive<om::Transform>();

    const auto& m = env.body.matrix;
    // OpenIGTLink TRANSFORM carries a 3×4 (12-element) affine; the
    // last 3 values are the translation (the "last 3 of matrix" in
    // the Python fixture's wording).
    const float x = m[m.size() - 3];
    const float y = m[m.size() - 2];
    const float z = m[m.size() - 1];

    om::Status reply;
    reply.code = 1;
    reply.subcode = 0;
    reply.error_name = "";
    reply.status_message =
        "got matrix[-3:]=[" +
        fmt_float(x) + ", " + fmt_float(y) + ", " + fmt_float(z) +
        "]";
    peer.send(env.device_name.empty() ? "echo" : env.device_name,
              reply);

    // Give the reply a moment to drain before we tear down the
    // socket.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    peer.stop();
    server.stop();
    return 0;
}
