"""Slow CPF (Phase Folding) then Pauli Compression (Methods A–E).

Pipeline::

    hamiltonian (.pauli/.qasm)
        → Phase Folding (to-zyz → to-tableau --fold)
        → after_phase_fold Pauli list
        → Methods A–E grid + experiment_best(F*)

Also supports the lighter path that skips fold and compresses the Hamiltonian
list directly (for mapping ablation controls).
"""

from __future__ import annotations

from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Sequence

from cpf_extract import extract_after_slow_cpf
from experiment_best import (
    DEFAULT_FIDELITY_TIERS,
    ExperimentBest,
    SweepResult,
    sweep_methods_ae,
)
from methods_ae import count_nonclifford
from pauli_list import PauliCircuit, load_pauli_file, write_pauli_file
from util import DEFAULT_QSYN_BIN


@dataclass
class SlowCompressResult:
    """One (benchmark, mapping) slow-fold → compress run."""

    name: str
    n_qubits: int
    n_rot_raw: int
    n_rot_after_phase_fold: int
    tiers: Dict[float, Optional[ExperimentBest]] = field(default_factory=dict)
    folded_pauli: str = ""
    sweep_n_points: int = 0

    def remaining_pct(self, f_target: float) -> Optional[float]:
        best = self.tiers.get(float(f_target))
        if best is None or self.n_rot_raw <= 0:
            return None
        return 100.0 * best.n_rot / self.n_rot_raw

    def summary_dict(self) -> dict:
        return {
            "name": self.name,
            "n_qubits": self.n_qubits,
            "n_rot_raw": self.n_rot_raw,
            "n_rot_after_phase_fold": self.n_rot_after_phase_fold,
            "sweep_n_points": self.sweep_n_points,
            "folded_pauli": self.folded_pauli,
            "experiment_best": {
                str(k): (None if v is None else v.summary_dict())
                for k, v in self.tiers.items()
            },
            "remaining_pct_of_raw": {
                str(k): self.remaining_pct(k) for k in self.tiers
            },
        }


def compress_pauli_list(
    pc: PauliCircuit,
    *,
    tiers: Sequence[float] = DEFAULT_FIDELITY_TIERS,
    adaptive_eps: bool = True,
) -> SweepResult:
    """Methods A–E sweep on an already-prepared Pauli list."""
    return sweep_methods_ae(pc, adaptive_eps=adaptive_eps)


def slow_cpf_then_compress(
    *,
    pauli_path: Path,
    qasm_path: Path,
    out_dir: Path,
    tiers: Sequence[float] = DEFAULT_FIDELITY_TIERS,
    qsyn_bin: Path = DEFAULT_QSYN_BIN,
    name: Optional[str] = None,
    skip_phase_fold: bool = False,
    adaptive_eps: bool = True,
    timeout_s: int = 14400,
) -> SlowCompressResult:
    """Hamiltonian files → (optional Phase Folding) → A–E ``experiment_best``.

    Parameters
    ----------
    skip_phase_fold:
        If True, compress the Hamiltonian Pauli list directly (no slow CPF).
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    raw = load_pauli_file(pauli_path, name=name or Path(pauli_path).stem)
    n_raw = count_nonclifford(raw)

    if skip_phase_fold:
        folded = raw
        folded_path = out_dir / "hamiltonian.pauli"
        write_pauli_file(folded, folded_path)
    else:
        folded = extract_after_slow_cpf(
            Path(qasm_path),
            name=raw.name,
            qsyn_bin=Path(qsyn_bin),
            raw_pauli=raw,
            timeout_s=timeout_s,
        )
        folded_path = out_dir / "after_phase_fold.pauli"
        write_pauli_file(folded, folded_path)

    sweep = compress_pauli_list(folded, tiers=tiers, adaptive_eps=adaptive_eps)
    table = sweep.experiment_best_table(tiers)
    return SlowCompressResult(
        name=raw.name,
        n_qubits=raw.n_qubits,
        n_rot_raw=n_raw,
        n_rot_after_phase_fold=count_nonclifford(folded),
        tiers=table,
        folded_pauli=str(folded_path),
        sweep_n_points=len(sweep.points),
    )


def mapping_slow_compress(
    benchmark: str,
    mapping: str,
    *,
    out_root: Path,
    tiers: Sequence[float] = DEFAULT_FIDELITY_TIERS,
    dt: float = 0.05,
    qsyn_bin: Path = DEFAULT_QSYN_BIN,
    skip_phase_fold: bool = False,
    allow_regen: bool = True,
) -> SlowCompressResult:
    """Generate mapping Hamiltonian, then slow CPF → Methods A–E."""
    from hamiltonian_mappings import prepare_mapping_benchmark

    mapping_dir = prepare_mapping_benchmark(
        Path(out_root) / "hamiltonian",
        benchmark,
        mapping,
        dt=dt,
        allow_regen=allow_regen,
    )
    label = mapping_dir.name
    return slow_cpf_then_compress(
        pauli_path=mapping_dir / f"{label}.pauli",
        qasm_path=mapping_dir / f"{label}.qasm",
        out_dir=Path(out_root) / "compress" / label,
        tiers=tiers,
        qsyn_bin=qsyn_bin,
        name=label,
        skip_phase_fold=skip_phase_fold,
    )
