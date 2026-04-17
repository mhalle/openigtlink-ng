#!/usr/bin/env bash
# Live interop gate: unmodified upstream ReceiveServer.cxx (linked
# only against our shim, not upstream's libOpenIGTLink.a) reads
# TRANSFORM messages from unmodified upstream TrackerClient.cxx
# (also via our shim).
#
# Passes if ReceiveServer prints at least one "Receiving TRANSFORM"
# line within a 2-second window — i.e., the full stack works:
# ClientSocket::ConnectToServer, Send, ServerSocket::WaitFor-
# Connection, Socket::Receive (header then body), MessageHeader::
# Unpack, TransformMessage dispatch.
#
# Invoked from CMake; expects three positional args:
#   $1 = path to upstream_ReceiveServer_via_shim
#   $2 = path to upstream_TrackerClient_via_shim
#   $3 = TCP port to use

set -u

SERVER="${1:?missing server binary path}"
CLIENT="${2:?missing client binary path}"
PORT="${3:?missing port}"

OUT=$(mktemp -t upstream_interop.XXXXXX)
trap "rm -f $OUT" EXIT

# Start the server; give it a moment to bind.
"$SERVER" "$PORT" > "$OUT" 2>&1 &
SPID=$!
sleep 0.3

# Start the client at 50 fps; TrackerClient runs forever, so cap
# with `timeout` to about a second's worth of traffic.
timeout 1 "$CLIENT" 127.0.0.1 "$PORT" 50 > /dev/null 2>&1 || true

# Tell the server to exit.
sleep 0.2
kill -TERM $SPID 2>/dev/null || true
wait $SPID 2>/dev/null || true

# Gate.
COUNT=$(grep -c 'Receiving TRANSFORM' "$OUT" || true)
if [ "$COUNT" -lt 5 ]; then
    echo "FAIL: expected >=5 TRANSFORM msgs, got $COUNT" >&2
    echo "--- ReceiveServer output ---" >&2
    cat "$OUT" >&2
    exit 1
fi
echo "OK: $COUNT TRANSFORM messages round-tripped end-to-end"
