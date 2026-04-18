// igtlServerSocket.h — drop-in facade for upstream's
// `igtl::ServerSocket`. Binds a listening TCP socket via
// oigtl::transport::tcp::listen() and yields accepted connections
// as ClientSocket instances.

#ifndef __igtlServerSocket_h
#define __igtlServerSocket_h

#include "igtlClientSocket.h"
#include "igtlMacro.h"
#include "igtlSocket.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

namespace igtl {

class IGTLCommon_EXPORT ServerSocket : public Socket {
 public:
    igtlTypeMacro(igtl::ServerSocket, igtl::Socket);
    igtlNewMacro(igtl::ServerSocket);

    /// Bind a listening socket on `port`. Returns 0 on success,
    /// -1 on failure. `port == 0` requests an ephemeral port;
    /// query with `GetServerPort()`.
    int CreateServer(int port);

    /// Block until a client connects, or `msec` milliseconds
    /// elapse (0 = no timeout, matches upstream). Returns a
    /// ClientSocket::Pointer on success, nullptr on timeout /
    /// error. Close the returned socket when done.
    ClientSocket::Pointer WaitForConnection(unsigned long msec = 0);

    /// Port the server is listening on (the actual port, not the
    /// one originally requested — useful after CreateServer(0)).
    int GetServerPort();

    // ---- Optional access restrictions ------------------------------
    //
    // Every method below is OPT-IN. A ServerSocket with none of
    // these called behaves identically to upstream: accept any
    // peer, unlimited concurrency, no idle timeout, no per-message
    // size cap.
    //
    // Calling these before CreateServer() means "restrictions take
    // effect from the first connection." Calling them AFTER
    // CreateServer() means "restrictions take effect for the next
    // accept()." Existing live connections are not affected by a
    // post-CreateServer policy change.
    //
    // See core-cpp/compat/MIGRATION.md section "Restricting which
    // peers may connect" for researcher-friendly prose and worked
    // examples.
    //
    // Platform: Linux, macOS, and Windows.

    /// Accept only connections from the machine running the
    /// server (loopback). Good for single-machine setups, dev
    /// work, never-on-network scenarios.
    void RestrictToThisMachineOnly();

    /// Accept only peers on the same IP subnet as one of this
    /// machine's network interfaces. Computed at call time; a
    /// new NIC plugged in later does not widen the allow-list
    /// (restart the server to pick it up).
    ///
    /// Prints a line to stderr at call time summarising which
    /// ranges got allowed, so operators can confirm intent.
    void RestrictToLocalSubnet();

    /// Same as RestrictToLocalSubnet() but restricted to a single
    /// named interface. Useful for multi-NIC machines where only
    /// one NIC is the "device network." Interface names are
    /// platform-specific:
    ///   Linux:   "eth0", "enp3s0", "wlan0", "tailscale0"
    ///   macOS:   "en0", "utun4"
    ///   Windows: "Ethernet", "Ethernet 2", "Tailscale"
    void RestrictToLocalSubnet(const std::string& interface_name);

    /// Add a single peer or hostname to the allow-list. Callable
    /// multiple times; each call extends the list (union).
    ///
    /// Accepted forms:
    ///   "10.1.2.42"          single IP (IPv4)
    ///   "::1"                single IP (IPv6)
    ///   "10.1.2.0/24"        CIDR range
    ///   "10.1.2.1-10.1.2.254" dash-separated range
    ///   "tracker.lab.local"  hostname (resolved via DNS here)
    ///
    /// Returns 1 on success, 0 on parse / resolution failure.
    int AllowPeer(const std::string& ip_or_hostname);

    /// Add an inclusive address range to the allow-list. Both
    /// endpoints must be the same family (both IPv4 or both
    /// IPv6) and first <= last. Researcher-friendly alternative
    /// to CIDR for people who prefer two IPs over prefix notation.
    int AllowPeerRange(const std::string& first_ip,
                       const std::string& last_ip);

    /// Cap the number of peers allowed simultaneously. A new
    /// connection over the cap is accepted briefly then closed.
    /// 0 means "unlimited" (default).
    void SetMaxSimultaneousClients(int n);

    /// Hang up on peers that go silent for longer than the given
    /// duration. Default 0 = never (keep indefinitely). Useful
    /// for dropped cables or crashed clients.
    void DisconnectIfSilentFor(std::chrono::seconds t);

    /// Refuse any single message with body_size larger than `n`
    /// bytes. 0 means "no cap" (default). Check runs before any
    /// body bytes are allocated.
    void SetMaxMessageSizeBytes(std::size_t n);

 protected:
    ServerSocket();
    ~ServerSocket() override;

    struct AcceptorPimpl;
    std::unique_ptr<AcceptorPimpl> m_AcceptorPimpl;

 private:
    ServerSocket(const ServerSocket&) = delete;
    void operator=(const ServerSocket&) = delete;
};

}  // namespace igtl

#endif  // __igtlServerSocket_h
