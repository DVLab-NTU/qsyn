#!/usr/bin/env python3
"""bqskit_compare.py -- compare qsyn output against a BQSKit recompile.

Workflow:

    1. Read a baseline QASM file (the *input* to whatever qsyn pipeline
       produced the comparison candidate).
    2. Run BQSKit's default `CompilationTask` with the same target gate
       set (U3 + CNOT) and optimisation level.
    3. Emit a single-line summary listing CNOT counts, total gate counts
       and depth for `bqskit-compiled`, `qsyn-compiled` and a baseline.

This intentionally does *not* enforce that BQSKit wins on every metric:
its purpose is to give the developer a reproducible reference number
when judging whether the qsyn pipeline is competitive.

Usage:

    python scripts/bqskit_compare.py \\
        --baseline   testcase/benchmark/qasm/foo.qasm \\
        --candidate  /tmp/foo.qsyn.qasm

Dependencies: `pip install bqskit qiskit numpy`.
"""

from __future__ import annotations

import argparse
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


@dataclass
class CircuitStats:
    name: str
    n_qubits: int
    n_gates: int
    n_cnots: int
    depth: int
    runtime_s: Optional[float] = None

    def to_row(self) -> str:
        rt = f"{self.runtime_s:6.2f}s" if self.runtime_s is not None else "   -- "
        return (f"{self.name:<22}  qubits={self.n_qubits:2d}  "
                f"gates={self.n_gates:5d}  cnots={self.n_cnots:5d}  "
                f"depth={self.depth:5d}  runtime={rt}")


def _qiskit_stats(qasm_path: Path, name: str) -> CircuitStats:
    """Use qiskit to count gates/cnots on a QASM file."""
    try:
        from qiskit import QuantumCircuit
    except ImportError as exc:
        raise SystemExit("bqskit_compare.py requires qiskit; pip install qiskit") from exc

    qc = QuantumCircuit.from_qasm_file(str(qasm_path))
    n_cnots = sum(1 for instr in qc.data if instr.operation.name in ("cx", "cnot"))
    return CircuitStats(
        name=name,
        n_qubits=qc.num_qubits,
        n_gates=len(qc.data),
        n_cnots=n_cnots,
        depth=qc.depth(),
    )


def _bqskit_recompile(qasm_path: Path, opt_level: int) -> CircuitStats:
    """Recompile via BQSKit using its default U3+CNOT target gate set."""
    try:
        from bqskit import compile as bqskit_compile
        from bqskit.ir import Circuit
        from bqskit.ir.gates import CNOTGate
    except ImportError as exc:
        raise SystemExit("bqskit_compare.py requires bqskit; pip install bqskit") from exc

    circuit = Circuit.from_file(str(qasm_path))
    start = time.perf_counter()
    out = bqskit_compile(circuit, optimization_level=opt_level)
    runtime = time.perf_counter() - start

    n_cnots = sum(1 for op in out if isinstance(op.gate, CNOTGate))
    return CircuitStats(
        name=f"bqskit (opt={opt_level})",
        n_qubits=out.num_qudits,
        n_gates=out.num_operations,
        n_cnots=n_cnots,
        depth=out.depth,
        runtime_s=runtime,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--baseline", type=Path, required=True,
                        help="original input QASM (fed to both pipelines)")
    parser.add_argument("--candidate", type=Path,
                        help="QASM produced by the qsyn pipeline to score against bqskit "
                             "(skip to only report BQSKit + baseline stats)")
    parser.add_argument("--opt-level", type=int, default=1,
                        help="BQSKit optimisation_level (0..4); higher = slower")
    parser.add_argument("--skip-bqskit", action="store_true",
                        help="omit the BQSKit recompile; only print baseline + candidate stats")
    args = parser.parse_args()

    rows = [_qiskit_stats(args.baseline, "baseline (input)")]
    if args.candidate is not None:
        rows.append(_qiskit_stats(args.candidate, "qsyn (candidate)"))
    if not args.skip_bqskit:
        rows.append(_bqskit_recompile(args.baseline, args.opt_level))

    for row in rows:
        print(row.to_row())

    if args.candidate is not None and not args.skip_bqskit:
        qsyn_cnots = rows[1].n_cnots
        bqskit_cnots = rows[2].n_cnots
        diff = qsyn_cnots - bqskit_cnots
        verdict = "tie" if diff == 0 else ("qsyn wins" if diff < 0 else "bqskit wins")
        print(f"\nCNOT delta (qsyn - bqskit) = {diff:+d}  -- {verdict}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
