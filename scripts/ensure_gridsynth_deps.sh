#!/usr/bin/env bash
# Ensure GMP/MPFR (and libgmpxx) are installed for GridSynth builds.
# Missing deps: install immediately (no prompt). Linux uses sudo apt install.

set -euo pipefail

case "${QSYN_ENABLE_GRIDSYNTH:-ON}" in
    OFF|off|0|false|FALSE) exit 0 ;;
esac

has_gmp_mpfr() {
    local -a inc_dirs=()
    local gmp_inc=""

    if command -v pkg-config >/dev/null 2>&1; then
        if pkg-config --exists gmp mpfr 2>/dev/null; then
            gmp_inc="$(pkg-config --variable=includedir gmp 2>/dev/null || true)"
            [[ -n "$gmp_inc" ]] && inc_dirs+=("$gmp_inc")
        fi
    fi

    inc_dirs+=(/usr/include /usr/local/include)
    if command -v brew >/dev/null 2>&1; then
        local brew_prefix
        brew_prefix="$(brew --prefix 2>/dev/null || true)"
        [[ -n "$brew_prefix" ]] && inc_dirs+=("${brew_prefix}/include")
    fi

    for inc in "${inc_dirs[@]}"; do
        [[ -f "${inc}/gmp.h" && -f "${inc}/mpfr.h" && -f "${inc}/gmpxx.h" ]] && return 0
    done
    return 1
}

run_as_root() {
    if [[ "$(id -u)" -eq 0 ]]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        echo "Error: need root or sudo to install GMP/MPFR." >&2
        return 1
    fi
}

install_linux() {
    export DEBIAN_FRONTEND=noninteractive

    if command -v apt >/dev/null 2>&1 || command -v apt-get >/dev/null 2>&1; then
        run_as_root apt install -y libgmp-dev libmpfr-dev pkg-config
        return $?
    fi

    if command -v dnf >/dev/null 2>&1; then
        run_as_root dnf install -y gmp-devel mpfr-devel pkg-config
        return $?
    fi

    if command -v yum >/dev/null 2>&1; then
        run_as_root yum install -y gmp-devel mpfr-devel pkg-config
        return $?
    fi

    echo "Error: unsupported Linux distro; install GMP/MPFR dev packages manually." >&2
    return 1
}

install_macos() {
    command -v brew >/dev/null 2>&1 || {
        echo "Error: brew install gmp mpfr" >&2
        return 1
    }
    HOMEBREW_NO_AUTO_UPDATE=1 brew install gmp mpfr
}

has_gmp_mpfr && exit 0

case "$(uname -s)" in
    Linux) install_linux ;;
    Darwin) install_macos ;;
    *)
        echo "Error: unsupported OS; install GMP/MPFR manually." >&2
        exit 1
        ;;
esac

has_gmp_mpfr || {
    echo "Error: GMP/MPFR still missing after install." >&2
    exit 1
}
