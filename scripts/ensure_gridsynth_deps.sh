#!/usr/bin/env bash

set -euo pipefail

case "${QSYN_ENABLE_GRIDSYNTH:-ON}" in
    OFF|off|0|false|FALSE) exit 0 ;;
esac

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="$(mktemp -d)"
trap 'rm -rf -- "$build_dir"' EXIT
cmake -S "$script_dir/../cmake/gridsynth-deps" -B "$build_dir" "$@"
