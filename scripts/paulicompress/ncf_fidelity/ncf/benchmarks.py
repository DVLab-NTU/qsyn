"""Load the 13 NCF benchmark Pauli-rotation lists and verify Table IV counts.

Each benchmark is a single Trotter step: a list of Pauli rotations
``exp(-i theta/2 P)``.  Defaults to the Proposed Flow reproduction bundle
(same Table IV counts); optional ``--bench-root`` / ``NCF_BENCH_ROOT``.
"""

from __future__ import annotations

import os
from dataclasses import dataclass
from typing import List

from . import config


@dataclass
class PauliRotation:
    pauli: str      # length-n string over {I,X,Y,Z}
    theta: float    # rotation angle for exp(-i theta/2 P)


@dataclass
class Benchmark:
    name: str
    n_qubits: int
    rotations: List[PauliRotation]

    @property
    def n_paulis(self) -> int:
        return len(self.rotations)


def _read_pauli_file(path: str) -> Benchmark:
    """Parse a ``.pauli`` file: lines ``<PauliString> <coeff>`` (``#`` comments)."""
    import ast

    rots: List[PauliRotation] = []
    n = 0
    name = os.path.basename(os.path.dirname(path))
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 2:
                continue
            pauli = parts[0]
            theta = float(ast.literal_eval(parts[1]))
            n = max(n, len(pauli))
            rots.append(PauliRotation(pauli, theta))
    # normalise all Pauli strings to width n (right-pad with I just in case)
    rots = [PauliRotation(r.pauli.ljust(n, "I"), r.theta) for r in rots]
    return Benchmark(name, n, rots)


def load_benchmark(name: str) -> Benchmark:
    """Load one benchmark, dropping all-identity terms (global phase)."""
    path = config.resolve_pauli_path(name)
    return load_benchmark_from_path(path, name=name)


def load_benchmark_from_path(path: str, name: str = "") -> Benchmark:
    """Load a ``.pauli`` file (raw or post-compression), dropping identity terms."""
    if not os.path.isfile(path):
        raise FileNotFoundError(f"Pauli list not found: {path}")
    bench = _read_pauli_file(path)
    ident = "I" * bench.n_qubits
    bench.rotations = [r for r in bench.rotations if set(r.pauli) != {"I"} and r.pauli != ident]
    bench.name = name or os.path.splitext(os.path.basename(path))[0]
    return bench


def verify_table_iv(names=None, tol_ratio: float = 0.02):
    """Return a list of (name, n_qubits, n_paulis, expected_n, expected_p, ok)."""
    if names is None:
        names = list(config.BENCHMARK_TABLE.keys())
    rows = []
    for name in names:
        exp_n, exp_p = config.BENCHMARK_TABLE[name]
        try:
            b = load_benchmark(name)
            # allow +-1 for identity-term handling and small generation diffs
            ok_n = (b.n_qubits == exp_n)
            ok_p = abs(b.n_paulis - exp_p) <= max(1, int(tol_ratio * exp_p))
            rows.append((name, b.n_qubits, b.n_paulis, exp_n, exp_p, ok_n and ok_p))
        except Exception as exc:  # noqa: BLE001
            rows.append((name, -1, -1, exp_n, exp_p, False))
    return rows


if __name__ == "__main__":
    print(f"{'benchmark':<20}{'n_q':>5}{'n_pauli':>9}{'exp_q':>7}{'exp_p':>8}  ok")
    for name, nq, npauli, en, ep, ok in verify_table_iv():
        flag = "OK" if ok else "XX"
        print(f"{name:<20}{nq:>5}{npauli:>9}{en:>7}{ep:>8}  {flag}")
