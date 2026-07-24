"""Compile a benchmark under one of the four NCF-paper methods.

Methods (Table V of arXiv:2510.13573):

* ``gridsyn``          -- baseline: every Pauli rotation is diagonalised to a
  single Rz and synthesised by gridsynth at eps = 0.001.
* ``rustiq_trasyn``    -- baseline: Rustiq re-optimises the Clifford (entangling)
  network; the N_P rotations are synthesised at eps = 0.001.  (Rotation T-count
  / fidelity equal gridsynth-level; only the Clifford network changes.)
* ``ncf1``             -- single-qubit NCF: rotations fused into single-qubit
  unitaries (window 4), synthesised at eps = 0.001 * N_P / N_u.
* ``ncf2``             -- two-qubit NCF: rotations fused into two-qubit unitaries
  (window 128), synthesised by Synthetiq at eps = 0.12.

Single-qubit synthesis uses gridsynth throughout.  Because gridsynth guarantees
operator distance <= eps, the achieved *fidelity* is synthesizer-independent, so
this is a faithful stand-in for the paper's Trasyn for the fidelity question
(only the T-*count* of the Trasyn rows is a gridsynth upper bound).
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Optional, Sequence, Tuple

import numpy as np

from . import config
from .benchmarks import Benchmark
from .clifford_gen import Gate
from .fuse import fuse
from .grouping import group_rotations
from .synth import SynthResult, eps_single, synth_1q, synth_2q


@dataclass
class CompiledGroup:
    pivots: List[int]
    clifford: List[Gate]
    ideal_U: np.ndarray
    synth: SynthResult
    n_pauli: int


@dataclass
class CompiledMethod:
    benchmark: str
    method: str
    n_qubits: int
    n_paulis: int
    n_unitaries: int
    eps: float
    groups: List[CompiledGroup]

    @property
    def t_count(self) -> int:
        return sum(g.synth.t_count for g in self.groups)

    @property
    def clifford_count(self) -> int:
        # synthesised Cliffords + the conjugation wrapper (C and C^dag)
        wrap = sum(2 * len(g.clifford) for g in self.groups)
        return wrap + sum(g.synth.clifford_count for g in self.groups)

    @property
    def product_fidelity(self) -> float:
        F = 1.0
        for g in self.groups:
            if not np.isnan(g.synth.fidelity):
                F *= g.synth.fidelity
        return F

    @property
    def product_fidelity_direct(self) -> float:
        """Misaligned ``|Tr(U†V)|/d`` (can under-estimate when Synthetiq permutes qubits)."""
        F = 1.0
        for g in self.groups:
            fd = getattr(g.synth, "fidelity_direct", g.synth.fidelity)
            if not np.isnan(fd):
                F *= fd
        return F

    @property
    def product_fidelity_worst(self) -> float:
        """Min aligned F among Synthetiq candidates, multiplied over groups."""
        F = 1.0
        for g in self.groups:
            fw = getattr(g.synth, "fidelity_worst", g.synth.fidelity)
            if not np.isnan(fw):
                F *= fw
        return F


_METHOD_MODE = {"gridsyn": "single", "ncf1": "single", "ncf2": "two",
                "rustiq_trasyn": "single"}


def compile_method(bench: Benchmark, method: str, limit_groups: Optional[int] = None,
                   progress: bool = False, syn_time: float = 8.0) -> CompiledMethod:
    if method not in _METHOD_MODE:
        raise ValueError(f"unknown method {method!r}")
    mode = _METHOD_MODE[method]
    max_members = 1 if method in ("gridsyn", "rustiq_trasyn") else None
    groups = group_rotations(bench.rotations, mode, max_members=max_members)
    n_u = len(groups)
    if limit_groups is not None:
        groups = groups[:limit_groups]

    if method == "ncf2":
        eps = config.SYNTHETIQ_EPS
    else:
        eps = eps_single(bench.n_paulis, n_u)

    compiled: List[CompiledGroup] = []
    for gi, g in enumerate(groups):
        fg = fuse(g)
        if len(fg.pivots) == 1:
            sr = synth_1q(fg.unitary, eps)
        elif len(fg.pivots) == 2:
            if method == "ncf2":
                sr = synth_2q(fg.unitary, eps, name=f"{bench.name}_{gi}", time_s=syn_time)
            else:
                # single-qubit method produced a 2-pivot group only if a pair
                # failed to fuse; synthesise the 4x4 with Synthetiq at same eps.
                sr = synth_2q(fg.unitary, eps, name=f"{bench.name}_{gi}", time_s=syn_time)
        else:
            raise RuntimeError(f"group reduced to {len(fg.pivots)} pivots (method {method})")
        compiled.append(CompiledGroup(fg.pivots, fg.clifford, fg.unitary, sr, fg.n_pauli))
        if progress and (gi + 1) % 50 == 0:
            print(f"    [{bench.name}/{method}] {gi + 1}/{len(groups)} groups", flush=True)

    return CompiledMethod(bench.name, method, bench.n_qubits, bench.n_paulis,
                          n_u, eps, compiled)


# --------------------------------------------------------------------------- #
#  full-circuit QASM emission (for LiH exact statevector fidelity)            #
# --------------------------------------------------------------------------- #
_INV = {"h": "h", "s": "sdg", "sdg": "s", "x": "x", "t": "tdg", "tdg": "t",
        "y": "y", "z": "z", "cx": "cx", "cz": "cz"}


def _emit_gate(name: str, glob_qubits: Sequence[int]) -> str:
    if name == "cx":
        return f"cx q[{glob_qubits[0]}], q[{glob_qubits[1]}];"
    if name == "cz":
        return f"cz q[{glob_qubits[0]}], q[{glob_qubits[1]}];"
    return f"{name} q[{glob_qubits[0]}];"


def emit_qasm(cm: CompiledMethod) -> str:
    """Emit the full synthesised circuit as OpenQASM 2.0.

    Group unitary G = C^dag * U_fused * C, so the circuit is: apply C, then the
    synthesised U_fused on the pivots, then C^dag.
    """
    lines = ["OPENQASM 2.0;", 'include "qelib1.inc";', f"qreg q[{cm.n_qubits}];"]
    for g in cm.groups:
        # C : gates in recorded order
        for name, qs in g.clifford:
            lines.append(_emit_gate(name, qs))
        # synthesised U_fused on the pivot qubits (local -> global)
        for name, lqs in g.synth.ops:
            gq = [g.pivots[i] for i in lqs]
            lines.append(_emit_gate(name, gq))
        # C^dag : inverse gates in reverse order
        for name, qs in reversed(g.clifford):
            lines.append(_emit_gate(_INV[name], qs))
    return "\n".join(lines) + "\n"
