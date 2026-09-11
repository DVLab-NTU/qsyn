#!/usr/bin/env bash

set -euo pipefail

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

case "$(uname -s)" in
    Linux) install_linux ;;
    Darwin) install_macos ;;
    *)
        echo "Error: unsupported OS; install GMP/MPFR manually." >&2
        exit 1
        ;;
esac
