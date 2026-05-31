#!/usr/bin/env bash
# Ensure GMP/MPFR (and libgmpxx) are installed for GridSynth builds.
# Called from the Makefile before configure and from cmake/cppgridsynth.cmake.

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

    inc_dirs+=(
        /usr/include
        /usr/local/include
    )
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

install_linux() {
    if command -v apt-get >/dev/null 2>&1; then
        echo "GridSynth: installing libgmp-dev libmpfr-dev (apt)..."
        if [[ "$(id -u)" -eq 0 ]]; then
            apt-get update -qq
            apt-get install -y libgmp-dev libmpfr-dev pkg-config
        elif command -v sudo >/dev/null 2>&1; then
            sudo apt-get update -qq
            sudo apt-get install -y libgmp-dev libmpfr-dev pkg-config
        else
            echo "Error: run as root or install manually:" >&2
            echo "  apt-get install -y libgmp-dev libmpfr-dev pkg-config" >&2
            return 1
        fi
        return 0
    fi

    if command -v dnf >/dev/null 2>&1; then
        echo "GridSynth: installing gmp-devel mpfr-devel (dnf)..."
        if [[ "$(id -u)" -eq 0 ]]; then
            dnf install -y gmp-devel mpfr-devel pkg-config
        elif command -v sudo >/dev/null 2>&1; then
            sudo dnf install -y gmp-devel mpfr-devel pkg-config
        else
            echo "Error: run as root or install manually:" >&2
            echo "  dnf install -y gmp-devel mpfr-devel pkg-config" >&2
            return 1
        fi
        return 0
    fi

    if command -v yum >/dev/null 2>&1; then
        echo "GridSynth: installing gmp-devel mpfr-devel (yum)..."
        if [[ "$(id -u)" -eq 0 ]]; then
            yum install -y gmp-devel mpfr-devel pkg-config
        elif command -v sudo >/dev/null 2>&1; then
            sudo yum install -y gmp-devel mpfr-devel pkg-config
        else
            echo "Error: run as root or install manually:" >&2
            echo "  yum install -y gmp-devel mpfr-devel pkg-config" >&2
            return 1
        fi
        return 0
    fi

    echo "Error: unsupported Linux distro; install GMP/MPFR development packages manually." >&2
    return 1
}

install_macos() {
    if ! command -v brew >/dev/null 2>&1; then
        echo "Error: Homebrew required. Install from https://brew.sh then run:" >&2
        echo "  brew install gmp mpfr" >&2
        return 1
    fi
    echo "GridSynth: installing gmp mpfr (Homebrew)..."
    HOMEBREW_NO_AUTO_UPDATE=1 brew install gmp mpfr
}

install_deps() {
    case "$(uname -s)" in
        Linux) install_linux ;;
        Darwin) install_macos ;;
        *)
            echo "Error: unsupported OS '$(uname -s)'. Install GMP and MPFR manually." >&2
            return 1
            ;;
    esac
}

if has_gmp_mpfr; then
    exit 0
fi

echo "GridSynth requires GMP, MPFR, and libgmpxx; none detected."
install_deps

if has_gmp_mpfr; then
    echo "GridSynth dependencies ready."
    exit 0
fi

echo "Error: GMP/MPFR still not found after install attempt." >&2
echo "  macOS  : brew install gmp mpfr" >&2
echo "  Debian : sudo apt install libgmp-dev libmpfr-dev pkg-config" >&2
echo "  Fedora : sudo dnf install gmp-devel mpfr-devel pkg-config" >&2
echo "Or disable GridSynth: cmake -DQSYN_ENABLE_GRIDSYNTH=OFF ..." >&2
exit 1
