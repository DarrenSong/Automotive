#!/bin/bash
# Log SocketCAN traffic with timestamps (run on HiKey970).
# Usage: ./log_can.sh [ifname]
# Example: ./log_can.sh can0

set -e
IF="${1:-can0}"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUT="${HOME}/can_log_${STAMP}.txt"

if ! ip link show "$IF" &>/dev/null; then
  echo "interface $IF not found. try: ip link show type can"
  exit 1
fi

echo "logging $IF -> $OUT"
echo "stop with Ctrl-C"
candump -t a "$IF" | tee "$OUT"
