"""NCF grouping (Section IV-A of the paper).

We greedily gather Pauli rotations into groups whose generated subgroup reduces
to a single qubit (single-qubit NCF) or two qubits (two-qubit NCF).  Membership
uses :func:`clifford_gen.num_pivots`.  Reordering is only performed across
*commuting* rotations, so the fused product is exactly equal to the original
sub-product (no Trotter error introduced by grouping).
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Sequence

from . import clifford_gen as cg
from .benchmarks import PauliRotation
from .tableau import SignedPauli


@dataclass
class Group:
    indices: List[int]                     # rotation indices in fused order
    paulis: List[SignedPauli]              # signed Paulis (original frame)
    thetas: List[float]

    @property
    def size(self) -> int:
        return len(self.indices)


def _greedy_group(
    spaulis: Sequence[SignedPauli],
    thetas: Sequence[float],
    max_pivots: int,
    window: int,
    max_members: int,
) -> List[Group]:
    n_rot = len(spaulis)
    used = [False] * n_rot
    groups: List[Group] = []
    for i in range(n_rot):
        if used[i]:
            continue
        used[i] = True
        gidx = [i]
        gp = [spaulis[i]]
        gt = [thetas[i]]
        skipped: List[int] = []
        j = i + 1
        while j < n_rot and j <= i + window and len(gidx) < max_members:
            if used[j]:
                j += 1
                continue
            cand = spaulis[j]
            # to move cand left into the group it must commute with all skipped
            if all(cand.commutes_with(spaulis[s]) for s in skipped):
                if cg.num_pivots(gp + [cand]) <= max_pivots:
                    gidx.append(j)
                    gp.append(cand)
                    gt.append(thetas[j])
                    used[j] = True
                else:
                    skipped.append(j)
            else:
                skipped.append(j)
            j += 1
        groups.append(Group(gidx, gp, gt))
    return groups


def group_rotations(
    rotations: Sequence[PauliRotation],
    mode: str,
    window: int = None,
    max_members: int = None,
) -> List[Group]:
    """Group rotations for ``mode`` in {"single", "two"}."""
    from . import config

    spaulis = [SignedPauli.from_string(r.pauli) for r in rotations]
    thetas = [r.theta for r in rotations]
    if mode == "single":
        w = window if window is not None else config.WINDOW_SINGLE
        mm = max_members if max_members is not None else 3
        return _greedy_group(spaulis, thetas, 1, w, mm)
    if mode == "two":
        w = window if window is not None else config.WINDOW_TWO
        mm = max_members if max_members is not None else 15
        return _greedy_group(spaulis, thetas, 2, w, mm)
    raise ValueError(f"unknown grouping mode {mode!r}")
