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

# Start the server; give it time to bind. CI runners (especially
# macOS) can take longer than a local dev box; poll for the port
# instead of guessing with a fixed sleep.
"$SERVER" "$PORT" > "$OUT" 2>&1 &
SPID=$!

# Wait for the server to actually listen. Try several times —
# macOS `nc -z` behaves differently from Linux's so we use a
# bash TCP-open redirect, which is portable across both runners.
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if (echo > /dev/tcp/127.0.0.1/"$PORT") 2>/dev/null; then
        break
    fi
    sleep 0.1
    if [ "$i" = "20" ]; then
        echo "FAIL: server never bound on port $PORT" >&2
        echo "--- ReceiveServer output ---" >&2
        cat "$OUT" >&2
        kill -TERM $SPID 2>/dev/null || true
        exit 1
    fi
done

# Start the client at 50 fps; TrackerClient runs forever, so cap
# with a bash-side kill (GNU `timeout` isn't installed on macOS
# runners, which silently swallowed the client earlier).
"$CLIENT" 127.0.0.1 "$PORT" 50 > /dev/null 2>&1 &
CPID=$!
sleep 2
kill -TERM $CPID 2>/dev/null || true
wait $CPID 2>/dev/null || true

# Tell the server to exit.
sleep 0.3
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
