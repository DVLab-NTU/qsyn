#!/usr/bin/env bash
# Benchmark GridSynth integration: execution time and T-count vs 3·log₂(1/ε).
#
# Usage:
#   ./scripts/benchmark_gridsynth.sh [--qsyn PATH]
#
# Memory-leak check (macOS/Linux with ASan-enabled build):
#   cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
#         -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
#         -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
#         -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address'
#   cmake --build build-asan --target qsyn unit-test
#   ASAN_OPTIONS=detect_leaks=1 ./build-asan/qsyn-unit-test '[gridsynth]'

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

QSYN=./qsyn
while [[ $# -gt 0 ]]; do
    case $1 in
        -q|--qsyn) QSYN=$2; shift 2 ;;
        -h|--help)
            sed -n '2,14p' "$0"
            exit 0
            ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

if [[ ! -x "$QSYN" ]]; then
    echo "Error: qsyn not found or not executable: $QSYN" >&2
    exit 1
fi

theoretical_bound() {
    python3 - "$1" <<'PY'
import math, sys
eps = float(sys.argv[1])
print(math.ceil(3 * math.log2(1 / eps)) + 8)
PY
}

echo "=== GridSynth benchmark (qsyn: $QSYN) ==="
printf "\n%-10s %-8s %-10s %-10s %-8s\n" "epsilon" "theta" "T-count" "bound" "time(s)"
printf "%-10s %-8s %-10s %-10s %-8s\n" "--------" "-----" "-------" "-----" "-------"

FAIL=0
for eps in 1e-4 1e-6 1e-8 1e-10; do
    for theta in "pi/8" "pi/12" "pi/16" "pi/32"; do
        OUT=$(/usr/bin/time -p "$QSYN" -c \
            "logger off; qcir new; qcir qubit add 1; qcir gate add rz -ph $theta 0; qcir gridsynth --epsilon $eps --seed 0; qcir print --stat; quit -f" \
            2>&1)

        T=$(echo "$OUT" | awk '/T-family/ {print $3}')
        REAL=$(echo "$OUT" | awk '/^real / {print $2}')
        BOUND=$(theoretical_bound "$eps")

        STATUS=OK
        if [[ "$T" -gt "$BOUND" ]]; then
            STATUS=FAIL
            FAIL=1
        fi

        printf "%-10s %-8s %-10s %-10s %-8s %s\n" "$eps" "$theta" "$T" "$BOUND" "$REAL" "$STATUS"
    done
done

echo
echo "=== Stress test (500 syntheses, ε=1e-8) ==="
/usr/bin/time -p "$QSYN" -c "logger off; qcir new; qcir qubit add 1; qcir gate add rz -ph pi/8 0; $(for i in $(seq 1 500); do printf 'qcir copy; qcir checkout %d; qcir gridsynth --epsilon 1e-8 --seed 0; ' "$i"; done) quit -f" 2>&1 | awk '/^real / {print "wall time:", $2, "s"}'

if [[ "$FAIL" -ne 0 ]]; then
    echo
    echo "T-count validation FAILED: one or more cases exceeded 3·log₂(1/ε)+8" >&2
    exit 1
fi

echo
echo "All T-count checks passed."
