"""Zero-sweep heuristic (thesis Chapter 4): min #non-Clifford Pauli rotations at F*.

Faithful port of ``scripts/pca_compress/min_pauli_rotation.py``:

* Stage 1 — lossless D-merge + F-cancel
* Stage 2 — greedy remove by fidelity cost ``w = -ln|cos(d)|``, ``d = |θ-Clifford|/2``
* Stage 3 — prefix sums for O(log n) multi-F* queries (``RotationBudget``)

Objective: ``F_approx = prod |cos(d_P)| >= F*`` while minimising surviving
non-Clifford rotations (and secondarily Cliffords introduced by snap).
"""

from __future__ import annotations

import math
from dataclasses import asdict, dataclass
from typing import Dict, List, Optional, Sequence, Tuple

from pauli_list import (
    HALF_PI,
    TWO_PI,
    PauliCircuit,
    Rotation,
    coeff_error,
    max_eps_for_l2_budget,
    method_D_merge,
    method_F_cancel,
)

_CLIFFORD_TOL = 1e-9
DEFAULT_TIERS = (0.999, 0.99, 0.90)


def fidelity_to_l2_budget(f_target: float) -> float:
    """Small-angle seed ``B* = sqrt(-2 ln F*)`` (exact stop uses F-cost)."""
    if f_target <= 0.0 or f_target > 1.0:
        raise ValueError(f"f_target must be in (0, 1], got {f_target}")
    if f_target >= 1.0:
        return 0.0
    return math.sqrt(-2.0 * math.log(f_target))


def _wrap(theta: float) -> float:
    return (theta + math.pi) % TWO_PI - math.pi


def _nearest_clifford(theta: float) -> Tuple[float, float]:
    th = _wrap(theta)
    nearest = round(th / HALF_PI) * HALF_PI
    return nearest, abs(th - nearest) / 2.0


@dataclass
class Candidate:
    idx: int
    pauli: str
    theta: float
    nearest: float
    d: float
    w: float
    snap_is_drop: bool


@dataclass
class Plan:
    name: str
    n_qubits: int
    f_target: float
    l2_budget_seed: float
    n_rot_before: int
    n_rot_after: int
    removed: int
    dropped_to_zero: int
    snapped_to_clifford: int
    clifford_added: int
    l2_used: float
    fidelity_exact: float
    est_t_count: int
    equiv_C_budget: float
    equiv_B_topk: int
    equiv_A_eps: float
    equiv_E_absorb_tol: float
    compressed: PauliCircuit

    def summary_dict(self) -> dict:
        d = asdict(self)
        d.pop("compressed", None)
        return d


def _lossless_merge(pc: PauliCircuit) -> PauliCircuit:
    return method_F_cancel(method_D_merge(pc))


def plan_min_pauli_rotation(
    pc: PauliCircuit,
    f_target: float,
    *,
    t_per_rot: int = 31,
    minimize_clifford: bool = True,
) -> Plan:
    """Single-point optimal plan at target fidelity (O(n log n))."""
    merged = _lossless_merge(pc)

    candidates: List[Candidate] = []
    clifford_fixed: List[Rotation] = []
    for i, r in enumerate(merged.rotations):
        nearest, d = _nearest_clifford(r.theta)
        if d <= _CLIFFORD_TOL / 2.0:
            clifford_fixed.append(r)
            continue
        w = -math.log(max(abs(math.cos(d)), 1e-300))
        drop = abs(nearest % TWO_PI) <= 1e-12 or abs((nearest % TWO_PI) - TWO_PI) <= 1e-12
        candidates.append(Candidate(i, r.pauli, r.theta, nearest, d, w, drop))

    n_before = len(candidates)
    budget_w = -math.log(f_target) if f_target > 0 else math.inf

    order = sorted(range(n_before), key=lambda j: candidates[j].w)
    used_w = 0.0
    used_l2sq = 0.0
    removed_set: set[int] = set()
    for j in order:
        w = candidates[j].w
        if used_w + w > budget_w + 1e-15:
            break
        used_w += w
        used_l2sq += candidates[j].d * candidates[j].d
        removed_set.add(j)

    dropped = 0
    snapped = 0
    survivors_removed = [candidates[j] for j in removed_set]
    kept: List[Rotation] = list(clifford_fixed)
    slack_w = budget_w - used_w
    for c in survivors_removed:
        if c.snap_is_drop:
            dropped += 1
            continue
        if minimize_clifford:
            d0 = abs(_wrap(c.theta)) / 2.0
            extra_w = (-math.log(max(abs(math.cos(d0)), 1e-300))) - c.w
            if extra_w <= slack_w + 1e-15:
                slack_w -= extra_w
                used_w += extra_w
                used_l2sq += d0 * d0 - c.d * c.d
                dropped += 1
                continue
        snapped += 1
        kept.append(Rotation(c.pauli, c.nearest))

    for j, c in enumerate(candidates):
        if j not in removed_set:
            kept.append(Rotation(c.pauli, c.theta))

    compressed = PauliCircuit(pc.name, pc.n_qubits, kept)
    _, l2_exact, fid_exact = coeff_error(merged.rotations, compressed.rotations)
    n_after = n_before - len(removed_set)

    equiv_C = math.sqrt(used_l2sq)
    equiv_B = n_after
    eps_a, _ = max_eps_for_l2_budget(merged, equiv_C) if equiv_C > 0 else (0.0, 0.0)
    snap_dists = [2.0 * candidates[j].d for j in removed_set]
    equiv_E = max(snap_dists) if snap_dists else 0.0

    return Plan(
        name=pc.name,
        n_qubits=pc.n_qubits,
        f_target=f_target,
        l2_budget_seed=fidelity_to_l2_budget(f_target) if 0 < f_target < 1 else 0.0,
        n_rot_before=n_before,
        n_rot_after=n_after,
        removed=len(removed_set),
        dropped_to_zero=dropped,
        snapped_to_clifford=snapped,
        clifford_added=snapped,
        l2_used=l2_exact,
        fidelity_exact=fid_exact,
        est_t_count=n_after * t_per_rot,
        equiv_C_budget=equiv_C,
        equiv_B_topk=equiv_B,
        equiv_A_eps=eps_a,
        equiv_E_absorb_tol=equiv_E,
        compressed=compressed,
    )


@dataclass
class RotationBudget:
    """Prefix-sum structure: O(log n) min-#rot queries at any F*."""

    name: str
    n_before: int
    w_sorted: List[float]
    prefix_w: List[float]
    prefix_d2: List[float]

    def min_rot_at_fidelity(self, f_target: float) -> Tuple[int, float, float]:
        import bisect

        if f_target <= 0.0:
            k = self.n_before
        elif f_target >= 1.0:
            k = 0
        else:
            budget_w = -math.log(f_target)
            tol = 1e-9 * max(1.0, budget_w)
            k = bisect.bisect_right(self.prefix_w, budget_w + tol) - 1
            k = max(0, min(k, self.n_before))
        fid = math.exp(-self.prefix_w[k]) if k >= 0 else 1.0
        l2 = math.sqrt(self.prefix_d2[k])
        return self.n_before - k, fid, l2


def build_budget(pc: PauliCircuit) -> RotationBudget:
    merged = _lossless_merge(pc)
    ws: List[float] = []
    ds2: List[float] = []
    for r in merged.rotations:
        _nearest, d = _nearest_clifford(r.theta)
        if d <= _CLIFFORD_TOL / 2.0:
            continue
        ws.append(-math.log(max(abs(math.cos(d)), 1e-300)))
        ds2.append(d * d)
    order = sorted(range(len(ws)), key=lambda j: ws[j])
    w_sorted = [ws[j] for j in order]
    d2_sorted = [ds2[j] for j in order]
    prefix_w = [0.0]
    prefix_d2 = [0.0]
    for w, d2 in zip(w_sorted, d2_sorted):
        prefix_w.append(prefix_w[-1] + w)
        prefix_d2.append(prefix_d2[-1] + d2)
    return RotationBudget(pc.name, len(w_sorted), w_sorted, prefix_w, prefix_d2)


def plan_all_tiers(
    pc: PauliCircuit,
    tiers: Sequence[float] = DEFAULT_TIERS,
    *,
    t_per_rot: int = 31,
    minimize_clifford: bool = True,
) -> Dict[float, Plan]:
    return {
        f: plan_min_pauli_rotation(
            pc, f, t_per_rot=t_per_rot, minimize_clifford=minimize_clifford
        )
        for f in tiers
    }
