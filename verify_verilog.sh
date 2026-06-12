#!/bin/bash
#
# Compile + run the pistorm testbench with Icarus Verilog (no Quartus or
# Docker needed). Exits non-zero if the testbench does not report PASSED.
# Run from anywhere.
set -euo pipefail

BASE_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$BASE_DIR/rtl"

tb="$(mktemp)"
iverilog -g2012 -o "$tb" pistorm_tb.v pistorm.v
out="$(vvp "$tb" 2>&1 || true)"
rm -f "$tb"
if grep -q '==== PASSED ====' <<<"$out"; then
  echo "PASS"
else
  echo "FAIL"
  echo "$out" | tail -20
  exit 1
fi
