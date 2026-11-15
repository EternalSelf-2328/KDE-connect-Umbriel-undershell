#!/usr/bin/env bash
# User-space Tailscale; no TUN, DNS changes, accepted routes, SSH or exit node.
set -euo pipefail
umask 077
TOOLS="${UMBRIEL_TAILSCALE_DIR:-$HOME/.local/libexec/umbriel-connect-tailscale}"
REMOTE="${UMBRIEL_REMOTE_BIN:-$HOME/.local/bin/umbriel-connect-remote}"
RUNTIME="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
STATE="${XDG_STATE_HOME:-$HOME/.local/state}/umbriel-connect-tailscale"
SOCKET="$RUNTIME/umbriel-tailscale.sock"
PORT="${UMBRIEL_REMOTE_PORT:-31594}"
[[ "$PORT" =~ ^[0-9]+$ && "$PORT" -ge 1024 && "$PORT" -le 65535 ]] || { echo "Invalid port" >&2; exit 2; }
cli() { "$TOOLS/tailscale" --socket="$SOCKET" "$@"; }
case "${1:---help}" in
  vpn)
    mkdir -p "$STATE"
    exec "$TOOLS/tailscaled" --tun=userspace-networking --state="$STATE/state" --socket="$SOCKET" --port=0 --no-logs-no-support
    ;;
  login)
    exec "$TOOLS/tailscale" --socket="$SOCKET" login --accept-dns=false --accept-routes=false \
      --ssh=false --exit-node= --advertise-exit-node=false --netfilter-mode=off --hostname=umbriel-connect --timeout=60s
    ;;
  pair)
    ADDRESS="$(cli ip -4)"
    python3 - "$ADDRESS" <<'PY'
import ipaddress,sys
assert ipaddress.ip_address(sys.argv[1]) in ipaddress.ip_network('100.64.0.0/10'), 'No valid Tailscale IPv4'
PY
    "$REMOTE" init
    exec "$REMOTE" pair-proxy "$ADDRESS" "$PORT"
    ;;
  start)
    "$REMOTE" serve 127.0.0.1 "$PORT" &
    RECEIVER_PID=$!
    "$TOOLS/tailscale" --socket="$SOCKET" serve --tcp="$PORT" "tcp://127.0.0.1:$PORT" &
    PROXY_PID=$!
    cleanup() { kill "$RECEIVER_PID" "$PROXY_PID" 2>/dev/null || true; wait "$RECEIVER_PID" "$PROXY_PID" 2>/dev/null || true; }
    trap cleanup EXIT
    trap 'exit 130' INT TERM
    set +e
    wait -n "$RECEIVER_PID" "$PROXY_PID"
    RESULT=$?
    exit "$RESULT"
    ;;
  status) cli status ;;
  *)
    echo "Usage: umbriel-remote-session vpn|login|pair|start|status"
    echo "1. vpn (leave running); 2. login (same account as Android); 3. pair; 4. start"
    echo "Starts no service automatically and never enables Tailscale Funnel."
    ;;
esac
