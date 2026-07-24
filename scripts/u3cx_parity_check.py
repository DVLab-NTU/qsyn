#!/usr/bin/env python3
"""Compare native qsyn U3+CX compile vs external BQSKit (--u3syn).

Usage (from repo root, after `cmake --build build`):
    python3 scripts/u3cx_parity_check.py testcase/pr10_pipeline/qasm/cpf_demo.qasm

Requires `bqskit` for the external leg; native-only stats still print if import fails.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


def run_qsyn(qsyn: Path, qasm: Path, extra: str) -> tuple[int, str]:
    script = f"qcir read {qasm}\nqcir to-u3cx {extra} -r\nqcir print -s\nquit -f\n"
    proc = subprocess.run(
        [str(qsyn)],
        input=script,
        text=True,
        capture_output=True,
        cwd=qsyn.parent.parent,
    )
    out = proc.stdout + proc.stderr
    gates = 0
    for line in out.splitlines():
        if "Total" in line and "gate" in line.lower():
            parts = line.split()
            for i, p in enumerate(parts):
                if p.isdigit() and i + 1 < len(parts) and "gate" in parts[i + 1].lower():
                    gates = int(p)
    return gates, out


def run_equiv(qsyn: Path, qasm: Path, extra: str) -> bool:
    script = (
        f"qcir read {qasm}\n"
        f"qcir to-u3cx {extra} -r\n"
        "qcir equiv 0\n"
        "quit -f\n"
    )
    proc = subprocess.run(
        [str(qsyn)],
        input=script,
        text=True,
        capture_output=True,
        cwd=qsyn.parent.parent,
    )
    out = proc.stdout + proc.stderr
    return "equivalent" in out.lower()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("qasm", type=Path)
    parser.add_argument("--qsyn", type=Path, default=Path("build/qsyn"))
    parser.add_argument("--opt", type=int, default=2)
    args = parser.parse_args()

    repo = Path(__file__).resolve().parent.parent
    qsyn = (repo / args.qsyn).resolve()
    qasm = args.qasm.resolve()
    if not qsyn.is_file():
        print(f"qsyn binary not found: {qsyn}", file=sys.stderr)
        return 1

    opt_flag = f"--opt-level {args.opt}"
    native_gates, _ = run_qsyn(qsyn, qasm, opt_flag)
    native_ok = run_equiv(qsyn, qasm, opt_flag)

    try:
        import bqskit  # noqa: F401
    except ImportError:
        print("native:", native_gates, "gates, equiv=", native_ok)
        print("bqskit: not installed; skip --u3syn leg")
        return 0

    u3_gates, _ = run_qsyn(qsyn, qasm, f"--u3syn {opt_flag}")
    u3_ok = run_equiv(qsyn, qasm, f"--u3syn {opt_flag}")

    print(f"qasm: {qasm.name}")
    print(f"  native (opt {args.opt}): {native_gates} gates, equiv={native_ok}")
    print(f"  --u3syn  (opt {args.opt}): {u3_gates} gates, equiv={u3_ok}")
    if native_ok and u3_ok:
        ratio = native_gates / max(u3_gates, 1)
        print(f"  gate ratio native/u3syn: {ratio:.2f}x")
    return 0 if native_ok and u3_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
