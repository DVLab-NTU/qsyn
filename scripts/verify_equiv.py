#!/usr/bin/env python3
"""verify_equiv.py -- exact equivalence check between two QASM 2.0 files.

This is the external counterpart of `qcir equiv` inside qsyn.  It uses
Qiskit's `Operator` to build the dense unitaries and compares them up to
a single global phase.  Used by:

    * PR-10's manual sanity checks (`python scripts/verify_equiv.py a.qasm b.qasm`)
    * PR-11's benchmark runner (imports `verify_equiv` programmatically).

Exit code is 0 on equivalence, non-zero otherwise so the script also
fits cleanly into shell pipelines and CI workflows.

Dependencies: qiskit >= 1.0 (any backend; only the Quantum Info module
is touched).  Install via `pip install qiskit numpy`.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np


def _load_qiskit():
    """Lazy import so the file is at least syntactically importable
    without qiskit installed (useful for `python -m py_compile`)."""
    try:
        from qiskit import QuantumCircuit
        from qiskit.quantum_info import Operator
    except ImportError as exc:
        raise SystemExit(
            "verify_equiv.py requires qiskit. Install with `pip install qiskit numpy`."
        ) from exc
    return QuantumCircuit, Operator


def load_unitary(path: Path):
    """Return the dense unitary (numpy 2D array) of the QASM file."""
    QuantumCircuit, Operator = _load_qiskit()
    qc = QuantumCircuit.from_qasm_file(str(path))
    return np.asarray(Operator(qc).data)


def is_equivalent(u1: np.ndarray, u2: np.ndarray, tol: float = 1e-7):
    """Check `u1 == e^{i*phi} * u2` for some real `phi`.

    Returns a tuple `(equal, residual)`.  `residual` is the Frobenius
    norm of the phase-corrected difference, useful for diagnostics.
    """
    if u1.shape != u2.shape:
        return False, float("inf")
    # Pick the first |U2[i,j]| > tol entry to anchor the phase.
    flat = np.argmax(np.abs(u2))
    i, j = np.unravel_index(flat, u2.shape)
    if abs(u2[i, j]) < tol:
        return np.allclose(u1, 0, atol=tol), float(np.linalg.norm(u1 - u2))
    phase = u1[i, j] / u2[i, j]
    phase /= abs(phase)
    residual = float(np.linalg.norm(u1 - phase * u2))
    return residual <= tol * max(1.0, np.linalg.norm(u1)), residual


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("lhs", type=Path, help="first QASM file")
    parser.add_argument("rhs", type=Path, help="second QASM file")
    parser.add_argument("--tol", type=float, default=1e-7,
                        help="absolute Frobenius tolerance on phase-corrected diff")
    parser.add_argument("--quiet", action="store_true",
                        help="suppress per-call logging; only the exit code")
    args = parser.parse_args()

    u1 = load_unitary(args.lhs)
    u2 = load_unitary(args.rhs)
    equal, residual = is_equivalent(u1, u2, args.tol)

    if not args.quiet:
        verdict = "EQUIVALENT" if equal else "NOT EQUIVALENT"
        print(f"{verdict}  residual={residual:.3e}  tol={args.tol:.1e}  "
              f"({args.lhs.name} vs {args.rhs.name})")
    return 0 if equal else 1


if __name__ == "__main__":
    sys.exit(main())
