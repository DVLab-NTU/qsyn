"""Compare Pauli Compression after Hamiltonian vs after Phase Folding.

Stage names follow the Proposed Flow::

    hamiltonian → (Phase Folding) → after_phase_fold → Pauli Compression

Does **not** use ``w/`` / ``w/o`` in APIs — those are slide shorthand only.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence

from baselines import (
    DEFAULT_MOLECULE_BENCHES,
    load_after_phase_fold,
    load_hamiltonian,
    iter_molecule_benches,
)
from experiment_best import (
    DEFAULT_FIDELITY_TIERS,
    ExperimentBest,
    SweepResult,
    sweep_methods_ae,
)


@dataclass
class StageCompressionSummary:
    stage: str  # "hamiltonian" | "after_phase_fold"
    n_rot_pre_compress: int
    experiment_best: Dict[float, Optional[ExperimentBest]] = field(default_factory=dict)

    def summary_dict(self) -> dict:
        return {
            "stage": self.stage,
            "n_rot_pre_compress": self.n_rot_pre_compress,
            "experiment_best": {
                str(k): (None if v is None else v.summary_dict())
                for k, v in self.experiment_best.items()
            },
        }


@dataclass
class PhaseFoldCompressionCompare:
    """One benchmark: compression on hamiltonian vs after_phase_fold lists."""

    bench: str
    n_qubits: int
    n_rot_hamiltonian: int
    n_rot_after_phase_fold: int
    hamiltonian: StageCompressionSummary
    after_phase_fold: StageCompressionSummary

    def summary_dict(self) -> dict:
        return {
            "bench": self.bench,
            "n_qubits": self.n_qubits,
            "n_rot_hamiltonian": self.n_rot_hamiltonian,
            "n_rot_after_phase_fold": self.n_rot_after_phase_fold,
            "hamiltonian": self.hamiltonian.summary_dict(),
            "after_phase_fold": self.after_phase_fold.summary_dict(),
        }


def _stage_summary(stage: str, sweep: SweepResult, tiers: Sequence[float]) -> StageCompressionSummary:
    return StageCompressionSummary(
        stage=stage,
        n_rot_pre_compress=sweep.n_rot_before,
        experiment_best=sweep.experiment_best_table(tiers),
    )


def compare_phase_fold_compression(
    bench: str,
    *,
    tiers: Sequence[float] = DEFAULT_FIDELITY_TIERS,
    bench_root=None,
    fold_root=None,
) -> PhaseFoldCompressionCompare:
    """Sweep Methods A–E on both stages and return experiment_best per F*."""
    from pathlib import Path

    kw_h = {} if bench_root is None else {"bench_root": Path(bench_root)}
    kw_f = {} if fold_root is None else {"fold_root": Path(fold_root)}
    pc_h = load_hamiltonian(bench, **kw_h)
    pc_f = load_after_phase_fold(bench, **kw_f)
    sw_h = sweep_methods_ae(pc_h)
    sw_f = sweep_methods_ae(pc_f)
    return PhaseFoldCompressionCompare(
        bench=bench,
        n_qubits=pc_h.n_qubits,
        n_rot_hamiltonian=pc_h.n_rot,
        n_rot_after_phase_fold=pc_f.n_rot,
        hamiltonian=_stage_summary("hamiltonian", sw_h, tiers),
        after_phase_fold=_stage_summary("after_phase_fold", sw_f, tiers),
    )


def compare_suite(
    benches: Optional[Sequence[str]] = None,
    *,
    tiers: Sequence[float] = DEFAULT_FIDELITY_TIERS,
    bench_root=None,
    fold_root=None,
) -> List[PhaseFoldCompressionCompare]:
    out: List[PhaseFoldCompressionCompare] = []
    for b in iter_molecule_benches(benches or DEFAULT_MOLECULE_BENCHES):
        out.append(
            compare_phase_fold_compression(
                b, tiers=tiers, bench_root=bench_root, fold_root=fold_root
            )
        )
    return out
