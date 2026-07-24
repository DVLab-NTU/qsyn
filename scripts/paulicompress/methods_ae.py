"""Methods A–E for Pauli-list compression (thesis Ch. 4 baselines).

All methods take a :class:`PauliCircuit` and return a new one.  Lossless
helpers ``method_D_merge`` / ``method_F_cancel`` live in ``pauli_list`` and
are re-exported here for a single import surface.

Counting convention for experiment tables: **non-Clifford** Pauli rotations
(angles not an integer multiple of π/2), matching the NCF sweep CSVs.
"""

from __future__ import annotations

import math
from typing import List, Sequence, Tuple

from pauli_list import (
    HALF_PI,
    TWO_PI,
    PauliCircuit,
    Rotation,
    method_D_merge,
    method_F_cancel,
)

__all__ = [
    "angle_is_clifford",
    "count_nonclifford",
    "method_A_truncate",
    "method_B_topk",
    "method_C_budget",
    "method_D_merge",
    "method_E_clifford",
    "method_F_cancel",
]


def _wrap(theta: float) -> float:
    return (theta + math.pi) % TWO_PI - math.pi


def angle_is_clifford(theta: float, tol: float = 1e-9) -> bool:
    r = theta % TWO_PI
    for k in range(5):
        if abs(r - k * HALF_PI) <= tol or abs(r - TWO_PI) <= tol:
            return True
    return False


def count_nonclifford(pc: PauliCircuit, tol: float = 1e-9) -> int:
    return sum(1 for r in pc.rotations if not angle_is_clifford(r.theta, tol=tol))


def method_A_truncate(pc: PauliCircuit, eps: float) -> PauliCircuit:
    """A: drop every rotation with ``|θ| < eps`` (after D-merge)."""
    merged = method_D_merge(pc)
    rots = [r for r in merged.rotations if abs(_wrap(r.theta)) >= eps]
    return PauliCircuit(pc.name, pc.n_qubits, rots)


def method_B_topk(pc: PauliCircuit, k: int) -> PauliCircuit:
    """B: keep the ``k`` rotations with largest ``|θ|`` (after D-merge)."""
    merged = method_D_merge(pc)
    ranked = sorted(merged.rotations, key=lambda r: abs(_wrap(r.theta)), reverse=True)
    return PauliCircuit(pc.name, pc.n_qubits, ranked[: max(0, int(k))])


def method_C_budget(pc: PauliCircuit, l2_budget: float) -> PauliCircuit:
    """C: greedily drop smallest-``|θ|`` terms while cumulative L2 ≤ budget.

    L2 uses coefficient error ``d = θ/2`` (same as ``coeff_error``).
    """
    merged = method_D_merge(pc)
    ranked = sorted(merged.rotations, key=lambda r: abs(_wrap(r.theta)))
    drop_l2sq = 0.0
    drop_idx: set[int] = set()
    for i, r in enumerate(ranked):
        c = _wrap(r.theta) / 2.0
        if math.sqrt(drop_l2sq + c * c) > l2_budget:
            break
        drop_l2sq += c * c
        drop_idx.add(i)
    kept = [r for i, r in enumerate(ranked) if i not in drop_idx]
    return PauliCircuit(pc.name, pc.n_qubits, kept)


def method_E_clifford(pc: PauliCircuit, eps: float) -> Tuple[PauliCircuit, int]:
    """E: absorb rotations within ``eps`` of a Clifford multiple of π/2.

    Returns ``(compressed, n_absorbed)``.  Absorbed terms are either dropped
    (nearest ≈ 0) or kept at the exact Clifford angle (no longer non-Clifford).
    """
    merged = method_D_merge(pc)
    kept: List[Rotation] = []
    absorbed = 0
    for r in merged.rotations:
        th = _wrap(r.theta)
        nearest = round(th / HALF_PI) * HALF_PI
        if abs(th - nearest) <= eps:
            absorbed += 1
            if abs(nearest % TWO_PI) > 1e-12 and abs((nearest % TWO_PI) - TWO_PI) > 1e-12:
                kept.append(Rotation(r.pauli, nearest))
        else:
            kept.append(r)
    return PauliCircuit(pc.name, pc.n_qubits, kept), absorbed


def adaptive_eps_values(pc: PauliCircuit) -> List[float]:
    """Per-circuit eps samples from the ``|θ|`` distribution (quantiles)."""
    merged = method_D_merge(pc)
    angs = sorted(abs(_wrap(r.theta)) for r in merged.rotations)
    if not angs:
        return []
    n = len(angs)
    picks: set[float] = set()
    for q in (0.05, 0.1, 0.2, 0.35, 0.5, 0.65, 0.8, 0.9, 0.95):
        picks.add(angs[min(n - 1, int(round(q * (n - 1))))])
    lo, hi = angs[0], angs[-1]
    for t in (0.25, 0.5, 0.75):
        picks.add(lo + t * (hi - lo))
    return sorted(p for p in picks if p > 0)
