"""HYBRID / RECURSIVE Pauli-list compression (thesis Ch. 4 unified methods).

Ported from ``scripts/pca_compress/compress_ncf.py``:

* **HYBRID** — lossless D/F (+ exact E) then one shared-L2 DROP/SNAP pass
  under simultaneous A/B/C/E constraints.
* **RECURSIVE** — iterate HYBRID under a global L2 budget (quadrature).
"""

from __future__ import annotations

import math
from typing import Dict, List, Optional, Sequence, Tuple

from methods_ae import (
    angle_is_clifford,
    count_nonclifford,
    method_E_clifford,
    method_F_cancel,
)
from pauli_list import HALF_PI, TWO_PI, PauliCircuit, Rotation

__all__ = [
    "method_HYBRID",
    "method_RECURSIVE",
    "DEFAULT_HYBRID_TOPK_FRAC",
]

DEFAULT_HYBRID_TOPK_FRAC = 0.75


def _wrap(theta: float) -> float:
    return (theta + math.pi) % TWO_PI - math.pi


def _nearest_clifford(theta: float) -> Tuple[float, float]:
    """Return ``(nearest_clifford_angle, snap_L2_cost)`` with cost ``|Δθ|/2``."""
    th = _wrap(theta)
    nearest = round(th / HALF_PI) * HALF_PI
    return nearest, abs(th - nearest) / 2.0


def _lossless_invariants(pc: PauliCircuit) -> Tuple[PauliCircuit, int]:
    """D/F merge+cancel plus zero-cost exact-Clifford E (``eps=0``)."""
    cur = method_F_cancel(pc)
    cur, nab = method_E_clifford(cur, 0.0)
    cur = method_F_cancel(cur)
    return cur, nab


def _drop_coeff_l2(rots: Sequence[Rotation], indices: Sequence[int]) -> float:
    return math.sqrt(sum((abs(_wrap(rots[i].theta)) / 2.0) ** 2 for i in indices))


def _mandatory_drop_set(
    n: int,
    rank: Dict[int, int],
    rots: Sequence[Rotation],
    k_cap: int,
    eps_a: float,
) -> set:
    """Indices forced out by method-B (top-k) and method-A (|theta| floor)."""
    out: set = set()
    for i in range(n):
        if rank[i] >= k_cap:
            out.add(i)
        elif (
            eps_a > 1e-15
            and abs(_wrap(rots[i].theta)) < eps_a
            and not angle_is_clifford(rots[i].theta)
        ):
            out.add(i)
    return out


def _hybrid_unified_lossy(
    rots: List[Rotation],
    l2_budget: float,
    eps_a: float,
    topk_frac: float,
) -> Tuple[List[Rotation], dict]:
    """One-shot lossy pass: simultaneous A/B/C/E constraints, shared L2 pool."""
    n = len(rots)
    if n == 0 or l2_budget <= 1e-15:
        return rots, {
            "l2_used": 0.0,
            "k_cap": n,
            "mandatory_drops": 0,
            "optional_drops": 0,
            "snapped": 0,
            "absorbed_to_clifford": 0,
            "dropped_to_zero": 0,
        }

    k_cap = n if topk_frac >= 1.0 - 1e-15 else max(1, int(round(topk_frac * n)))
    order = sorted(range(n), key=lambda i: abs(_wrap(rots[i].theta)), reverse=True)
    rank = {i: r for r, i in enumerate(order)}

    while k_cap <= n:
        mandatory = _mandatory_drop_set(n, rank, rots, k_cap, eps_a)
        if _drop_coeff_l2(rots, mandatory) <= l2_budget + 1e-15:
            break
        k_cap += 1
    else:
        k_cap = n
        mandatory = _mandatory_drop_set(n, rank, rots, k_cap, eps_a)

    used_sq = sum((abs(_wrap(rots[i].theta)) / 2.0) ** 2 for i in mandatory)
    action: Dict[int, Tuple[str, Optional[float]]] = {
        i: ("drop", None) for i in mandatory
    }

    optional: List[Tuple[float, int, str, int, Optional[float]]] = []
    for i in range(n):
        if i in mandatory:
            continue
        if angle_is_clifford(rots[i].theta):
            action[i] = ("keep", None)
            continue
        dc = abs(_wrap(rots[i].theta)) / 2.0
        nearest, sc = _nearest_clifford(rots[i].theta)
        optional.append((dc, 1, "drop", i, None))
        if sc > 1e-15:
            optional.append((sc, 0, "snap", i, nearest))
    optional.sort(key=lambda t: (t[0], t[1], -abs(_wrap(rots[t[3]].theta))))

    opt_drop = opt_snap = 0
    for cost, _prio, kind, i, nearest in optional:
        if i in action:
            continue
        if used_sq + cost * cost > l2_budget * l2_budget + 1e-15:
            continue
        used_sq += cost * cost
        action[i] = (kind, nearest)
        if kind == "drop":
            opt_drop += 1
        else:
            opt_snap += 1

    kept: List[Rotation] = []
    absorbed = dropped = 0
    for i in range(n):
        act = action.get(i, ("keep", None))
        kind, nearest = act
        if kind == "drop":
            dropped += 1
            continue
        if kind == "snap" and nearest is not None:
            nz = nearest % TWO_PI
            if abs(nz) > 1e-12 and abs(nz - TWO_PI) > 1e-12:
                kept.append(Rotation(rots[i].pauli, nearest))
                absorbed += 1
            else:
                dropped += 1
            continue
        kept.append(rots[i])

    st = {
        "l2_used": math.sqrt(used_sq),
        "k_cap": k_cap,
        "mandatory_drops": len(mandatory),
        "optional_drops": opt_drop,
        "snapped": opt_snap,
        "absorbed_to_clifford": absorbed,
        "dropped_to_zero": dropped,
    }
    return kept, st


def method_HYBRID(
    pc: PauliCircuit,
    l2_budget: float,
    eps_a: float = 0.0,
    topk_frac: float = 1.0,
    max_rounds: int = 6,  # API compat; RECURSIVE owns outer iteration
) -> Tuple[PauliCircuit, dict]:
    """HYBRID: one algorithm, simultaneous A~F constraints (not sequential A→F)."""
    _ = max_rounds
    cur, e_exact = _lossless_invariants(pc)
    e_near = 0
    if eps_a > 1e-15:
        cur, e_near = method_E_clifford(cur, eps_a)
        cur = method_F_cancel(cur)
        e_exact += e_near
    rots = [Rotation(r.pauli, r.theta) for r in cur.rotations]

    base_stats = {
        "start": len(pc.rotations),
        "after_cancel": len(method_F_cancel(pc).rotations),
        "e_exact_absorbed": e_exact,
        "e_near_absorbed": e_near,
    }

    if l2_budget <= 1e-15 and eps_a <= 1e-15 and topk_frac >= 1.0 - 1e-15:
        out = method_F_cancel(PauliCircuit(pc.name, pc.n_qubits, rots))
        return out, {
            **base_stats,
            "l2_used": 0.0,
            "k_cap": len(rots),
            "mandatory_drops": 0,
            "optional_drops": 0,
            "snapped": 0,
            "absorbed_to_clifford": 0,
            "dropped_to_zero": 0,
        }

    kept, st = _hybrid_unified_lossy(rots, l2_budget, eps_a, topk_frac)
    out = method_F_cancel(PauliCircuit(pc.name, pc.n_qubits, kept))
    return out, {**base_stats, **st}


def method_RECURSIVE(
    pc: PauliCircuit,
    l2_budget: float,
    eps_a: float = 0.0,
    topk_frac: float = 1.0,
    max_iter: int = 15,
) -> Tuple[PauliCircuit, dict]:
    """RECURSIVE: repeat HYBRID to a fixpoint under one global L2 budget."""
    if l2_budget <= 1e-15 and eps_a <= 1e-15 and topk_frac >= 1.0 - 1e-15:
        cur, _ = _lossless_invariants(pc)
        cur = method_F_cancel(cur)
        return cur, {
            "history": [count_nonclifford(pc), count_nonclifford(cur)],
            "iters": 1,
            "l2_used": 0.0,
        }

    cur = pc
    used_sq = 0.0
    history = [count_nonclifford(cur)]
    outer = 0
    for _ in range(max_iter):
        remaining = math.sqrt(max(0.0, l2_budget * l2_budget - used_sq))
        if remaining <= 1e-15:
            break
        cur = method_F_cancel(cur)
        nxt, st = method_HYBRID(
            cur, remaining, eps_a=eps_a, topk_frac=topk_frac
        )
        used_sq += st["l2_used"] * st["l2_used"]
        ncnt = count_nonclifford(nxt)
        history.append(ncnt)
        outer += 1
        cur = nxt
        if len(history) >= 3 and history[-1] == history[-2] == history[-3]:
            break
    return cur, {
        "history": history,
        "iters": outer,
        "l2_used": math.sqrt(used_sq),
    }
