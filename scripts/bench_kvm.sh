#!/bin/sh
# Simple benchmark comparing PCem with and without KVM.
# Usage: bench_kvm.sh <pcem_binary> <config_file>

set -e

PCEM="$1"
CFG="$2"

if [ -z "$PCEM" ] || [ -z "$CFG" ]; then
    echo "Usage: $0 <pcem_binary> <config_file>" >&2
    exit 1
fi

echo "Benchmarking with KVM..."
/usr/bin/time -f '%E real' "$PCEM" --kvm --config "$CFG" 2>/dev/null || true

echo "Benchmarking without KVM..."
/usr/bin/time -f '%E real' "$PCEM" --config "$CFG" 2>/dev/null || true
