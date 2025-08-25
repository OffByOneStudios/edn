#!/usr/bin/env bash
# Lightweight DI / IR pattern verification script (Phase I tooling task)
# Usage: ./scripts/di_grep_check.sh <llvm-ir-file> <pattern1> [pattern2...]
set -euo pipefail
if [ $# -lt 2 ]; then
  echo "Usage: $0 <ir-file> <pattern1> [pattern2 ...]" >&2
  exit 1
fi
IR=$1; shift
if [ ! -f "$IR" ]; then
  echo "IR file not found: $IR" >&2
  exit 2
fi
missing=0
for p in "$@"; do
  if ! grep -q -- "$p" "$IR"; then
    echo "[di_grep_check] MISSING pattern: $p" >&2
    missing=1
  fi
done
if [ $missing -ne 0 ]; then
  echo "[di_grep_check] One or more patterns missing" >&2
  exit 3
fi
echo "[di_grep_check] All patterns present"
