"""F* grid sweep over Methods A–E + ``experiment_best`` aggregation.

``experiment_best(F*)`` = minimum non-Clifford Pauli-rotation count among all
A–E (and lossless D/F) grid points with ``F_approx >= F*``.

This is the aggregator used for thesis / slide fold-ablation tables
(Hamiltonian → optional Phase Folding → Pauli Compression).
"""

from __future__ import annotations

from dataclasses import asdict, dataclass, field
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

from methods_ae import (
    adaptive_eps_values,
    count_nonclifford,
    method_A_truncate,
    method_B_topk,
    method_C_budget,
    method_D_merge,
    method_E_clifford,
    method_F_cancel,
)
from pauli_list import PauliCircuit, coeff_error

# Default grids (match historical ``compress_ncf.py`` NCF sweep).
DEFAULT_EPS_LIST: Tuple[float, ...] = (
    1e-6,
    1e-5,
    1e-4,
    5e-4,
    1e-3,
    2e-3,
    3e-3,
    5e-3,
    1e-2,
    2e-2,
    3e-2,
    4e-2,
    5e-2,
    7.5e-2,
    0.1,
    0.12,
    0.15,
)
DEFAULT_L2_BUDGETS: Tuple[float, ...] = (
    1e-5,
    1e-4,
    5e-4,
    1e-3,
    2e-3,
    5e-3,
    1e-2,
    2e-2,
    3e-2,
    5e-2,
    7.5e-2,
    0.1,
    0.12,
    0.15,
)
DEFAULT_TOPK_FRACS: Tuple[float, ...] = (0.25, 0.5, 0.75)
DEFAULT_FIDELITY_TIERS: Tuple[float, ...] = (0.999, 0.99, 0.9)


@dataclass
class SweepPoint:
    method: str
    setting: str
    n_rot_before: int
    n_rot_after: int
    approx_fidelity: float
    l1_err: float
    l2_err: float
    note: str = ""
    compressed: Optional[PauliCircuit] = None

    def summary_dict(self) -> dict:
        d = asdict(self)
        d.pop("compressed", None)
        return d


@dataclass
class ExperimentBest:
    fidelity: float
    n_rot: int
    method: str
    setting: str
    approx_fidelity: float
    l2_err: float
    compressed: Optional[PauliCircuit] = None

    def summary_dict(self) -> dict:
        d = asdict(self)
        d.pop("compressed", None)
        return d


@dataclass
class MethodsAePlan:
    """Winning A–E circuit at a single F* (for end-to-end synthesis)."""

    name: str
    n_qubits: int
    f_target: float
    n_rot_before: int
    n_rot_after: int
    method: str
    setting: str
    approx_fidelity: float
    l2_err: float
    compressed: PauliCircuit

    def summary_dict(self) -> dict:
        d = asdict(self)
        d.pop("compressed", None)
        return d


@dataclass
class SweepResult:
    name: str
    n_qubits: int
    n_rot_before: int
    points: List[SweepPoint] = field(default_factory=list)

    def _best_point(self, f_target: float) -> Optional[SweepPoint]:
        best: Optional[SweepPoint] = None
        for p in self.points:
            if p.approx_fidelity + 1e-12 < f_target:
                continue
            if best is None or p.n_rot_after < best.n_rot_after:
                best = p
            elif (
                best is not None
                and p.n_rot_after == best.n_rot_after
                and p.approx_fidelity > best.approx_fidelity
            ):
                best = p
        return best

    def experiment_best(self, f_target: float) -> Optional[ExperimentBest]:
        """Min ``n_rot_after`` among points with ``approx_fidelity >= f_target``."""
        best = self._best_point(f_target)
        if best is None:
            return None
        return ExperimentBest(
            fidelity=f_target,
            n_rot=best.n_rot_after,
            method=best.method,
            setting=best.setting,
            approx_fidelity=best.approx_fidelity,
            l2_err=best.l2_err,
            compressed=best.compressed.copy() if best.compressed is not None else None,
        )

    def experiment_best_table(
        self, tiers: Sequence[float] = DEFAULT_FIDELITY_TIERS
    ) -> Dict[float, Optional[ExperimentBest]]:
        return {float(f): self.experiment_best(float(f)) for f in tiers}


def _record(
    baseline: PauliCircuit,
    n_before: int,
    method: str,
    setting: str,
    compressed: PauliCircuit,
    note: str = "",
) -> SweepPoint:
    l1, l2, fid = coeff_error(baseline.rotations, compressed.rotations)
    return SweepPoint(
        method=method,
        setting=setting,
        n_rot_before=n_before,
        n_rot_after=count_nonclifford(compressed),
        approx_fidelity=fid,
        l1_err=l1,
        l2_err=l2,
        note=note,
        compressed=compressed,
    )


def sweep_methods_ae(
    pc: PauliCircuit,
    *,
    eps_list: Sequence[float] = DEFAULT_EPS_LIST,
    l2_budgets: Sequence[float] = DEFAULT_L2_BUDGETS,
    topk_fracs: Sequence[float] = DEFAULT_TOPK_FRACS,
    adaptive_eps: bool = True,
) -> SweepResult:
    """Run Methods A–E (+ lossless D/F) on ``pc`` and collect sweep points."""
    baseline = pc
    n_before = count_nonclifford(baseline)
    points: List[SweepPoint] = []

    points.append(
        _record(baseline, n_before, "D-merge", "lossless", method_D_merge(pc))
    )
    points.append(
        _record(baseline, n_before, "F-cancel", "lossless", method_F_cancel(pc))
    )

    all_eps = list(eps_list)
    if adaptive_eps:
        all_eps = sorted(set(float(e) for e in all_eps) | set(adaptive_eps_values(pc)))

    for eps in all_eps:
        points.append(
            _record(
                baseline,
                n_before,
                "A-truncate",
                f"eps={eps:g}",
                method_A_truncate(pc, eps),
            )
        )
        comp_e, nab = method_E_clifford(pc, eps)
        points.append(
            _record(
                baseline,
                n_before,
                "E-clifford",
                f"eps={eps:g}",
                comp_e,
                note=f"absorbed={nab}",
            )
        )

    n_unique = method_D_merge(pc).n_rot
    for frac in topk_fracs:
        k = max(1, int(round(float(frac) * n_unique))) if n_unique else 0
        points.append(
            _record(
                baseline,
                n_before,
                "B-topk",
                f"k={k}({frac:g})",
                method_B_topk(pc, k),
            )
        )

    for bud in l2_budgets:
        points.append(
            _record(
                baseline,
                n_before,
                "C-budget",
                f"l2<={bud:g}",
                method_C_budget(pc, float(bud)),
            )
        )

    return SweepResult(
        name=pc.name,
        n_qubits=pc.n_qubits,
        n_rot_before=n_before,
        points=points,
    )


def plan_methods_ae(
    pc: PauliCircuit,
    f_target: float,
    *,
    eps_list: Sequence[float] = DEFAULT_EPS_LIST,
    l2_budgets: Sequence[float] = DEFAULT_L2_BUDGETS,
    topk_fracs: Sequence[float] = DEFAULT_TOPK_FRACS,
    adaptive_eps: bool = True,
) -> MethodsAePlan:
    """Pick the A–E grid winner at ``F*`` and return its compressed circuit."""
    sweep = sweep_methods_ae(
        pc,
        eps_list=eps_list,
        l2_budgets=l2_budgets,
        topk_fracs=topk_fracs,
        adaptive_eps=adaptive_eps,
    )
    best = sweep.experiment_best(f_target)
    if best is None or best.compressed is None:
        raise RuntimeError(
            f"no Methods A–E point satisfies F_approx >= {f_target} "
            f"for circuit {pc.name!r} (n_rot_before={sweep.n_rot_before})"
        )
    return MethodsAePlan(
        name=pc.name,
        n_qubits=pc.n_qubits,
        f_target=f_target,
        n_rot_before=sweep.n_rot_before,
        n_rot_after=best.n_rot,
        method=best.method,
        setting=best.setting,
        approx_fidelity=best.approx_fidelity,
        l2_err=best.l2_err,
        compressed=best.compressed,
    )


def experiment_best_at(
    points: Iterable[SweepPoint],
    f_target: float,
) -> Optional[ExperimentBest]:
    """Convenience wrapper when only a point list is available."""
    tmp = SweepResult(name="", n_qubits=0, n_rot_before=0, points=list(points))
    return tmp.experiment_best(f_target)
