#!/usr/bin/env python3

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile


def main():
    repo = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="qsyn-deps-test-") as directory:
        root = Path(directory)
        source = root / "source"
        shutil.copytree(repo / "cmake/gridsynth-deps", source / "cmake/gridsynth-deps")
        (source / "scripts").mkdir()
        installer = source / "scripts/install_gridsynth_deps.sh"
        marker = root / "installer-calls"
        installer.write_text(f"echo called >> {shlex.quote(str(marker))}\nexit 17\n")

        def configure(name, *options, succeeds=True):
            result = subprocess.run(
                ["cmake", "-S", str(source / "cmake/gridsynth-deps"),
                 "-B", str(root / name), *options],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            )
            assert (result.returncode == 0) == succeeds, result.stdout
            print(f"PASS: {name}")
            return result.stdout

        configure("installed")
        assert not marker.exists()
        cache = {}
        for line in (root / "installed/CMakeCache.txt").read_text().splitlines():
            if line.startswith("GRIDSYNTH_") and "=" in line:
                key, value = line.split("=", 1)
                cache[key.split(":", 1)[0]] = value

        payload = root / "payload"
        for library, header in [("GMP", "gmp.h"), ("GMPXX", "gmpxx.h"), ("MPFR", "mpfr.h")]:
            include = payload / "usr/include" / library.lower()
            include.mkdir(parents=True)
            shutil.copy2(Path(cache[f"GRIDSYNTH_{library}_INCLUDE"]) / header, include / header)
            libdir = payload / "usr/lib"
            libdir.mkdir(exist_ok=True)
            lib = Path(cache[f"GRIDSYNTH_{library}_LIB"])
            shutil.copy2(lib, libdir / lib.name)

        prefix = root / "prefix"
        options = [
            f"-DCMAKE_FIND_ROOT_PATH={prefix}",
            "-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY",
            "-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY",
            "-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=TRUE",
            "-DCMAKE_INCLUDE_PATH=/usr/include/gmp;/usr/include/gmpxx;/usr/include/mpfr",
        ]
        shutil.copytree(payload, prefix)
        configure("split-headers", *options)
        assert not marker.exists()

        mpfr_header = prefix / "usr/include/mpfr/mpfr.h"
        mpfr_header.unlink()
        output = configure("installer-failure", *options, succeeds=False)
        assert "status 17" in output, output
        assert marker.read_text().splitlines() == ["called"]
        marker.unlink()

        installer.write_text(f"echo called >> {shlex.quote(str(marker))}\n")
        output = configure("still-missing", *options, succeeds=False)
        assert "still missing after installation" in output, output
        assert marker.read_text().splitlines() == ["called"]
        marker.unlink()

        installer.write_text(
            f"echo called >> {shlex.quote(str(marker))}\n"
            f"cp {shlex.quote(str(payload / 'usr/include/mpfr/mpfr.h'))} "
            f"{shlex.quote(str(mpfr_header))}\n"
        )
        configure("auto-install", *options)
        assert marker.read_text().splitlines() == ["called"]
        marker.unlink()
        configure("auto-install", *options)
        assert not marker.exists()

        mpfr_lib = prefix / "usr/lib" / Path(cache["GRIDSYNTH_MPFR_LIB"]).name
        mpfr_lib.write_text("not a library\n")
        output = configure("broken-link", *options, "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY", succeeds=False)
        assert "compile/link check failed" in output, output
        assert str(mpfr_lib) in output, output
        assert not marker.exists()

        configure("disabled", *options, "-DQSYN_ENABLE_GRIDSYNTH=OFF", "-DCMAKE_CXX_COMPILER=missing-compiler")
        assert not marker.exists()
        subprocess.run(
            ["bash", str(repo / "scripts/ensure_gridsynth_deps.sh")],
            env={**os.environ, "QSYN_ENABLE_GRIDSYNTH": "OFF"}, check=True,
        )


if __name__ == "__main__":
    main()
