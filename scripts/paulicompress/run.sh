#!/usr/bin/env bash
# Thin wrapper: ./scripts/paulicompress/run.sh all --bench LiH
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
exec python3 "$ROOT/scripts/paulicompress/cli.py" "$@"
