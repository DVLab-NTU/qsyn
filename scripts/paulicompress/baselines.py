"""Stable Pauli-list baselines for the Proposed Flow front-end stages.

Flow stages (names used in APIs / CLI)::

    hamiltonian          # Paulihedral / lattice Trotter list (.pauli)
        → Phase Folding  # slow CPF: to-zyz → to-tableau --fold
    after_phase_fold     # folded Pauli list
        → Pauli Compression (Heuristic zero-sweep; optional methods-ae / …)

``hamiltonian`` lists come from ``prepare`` / the reproduction bundle.
``after_phase_fold`` lists are shipped under ``baselines/after_phase_fold/``
(frozen slow-CPF extracts) so developers can iterate on compression without
re-running expensive fold on large molecules.
"""

from __future__ import annotations

from pathlib import Path
from typing import Iterable, List, Optional, Sequence, Tuple

from pauli_list import PauliCircuit, load_pauli_file
from util import QSYN_ROOT

HERE = Path(__file__).resolve().parent
BASELINES_ROOT = HERE / "baselines"
AFTER_PHASE_FOLD_DIR = BASELINES_ROOT / "after_phase_fold"

# Bundled molecular suite used in the Phase Folding ablation slides / §6.4.2.
DEFAULT_MOLECULE_BENCHES: Tuple[str, ...] = ("LiH", "H2O", "N2", "H2S", "CO2")

_HAMILTONIAN_CANDIDATES = (
    QSYN_ROOT
    / "scripts"
    / "pca_compress"
    / "reproduction_bundle_proposed_flow"
    / "01_original_benchmarks",
    QSYN_ROOT / "out" / "01_original_benchmarks",
    Path("out/01_original_benchmarks"),
    Path("scripts/pca_compress/reproduction_bundle_proposed_flow/01_original_benchmarks"),
)


class BaselineNotFoundError(FileNotFoundError):
    pass


def list_after_phase_fold_benches(
    root: Path = AFTER_PHASE_FOLD_DIR,
) -> List[str]:
    root = Path(root)
    if not root.is_dir():
        return []
    return sorted(p.stem for p in root.glob("*.pauli"))


def resolve_hamiltonian_pauli(
    bench: str,
    *,
    bench_root: Optional[Path] = None,
) -> Path:
    """Locate ``<bench>/<bench>.pauli`` for the Hamiltonian stage."""
    roots: List[Path] = []
    if bench_root is not None:
        roots.append(Path(bench_root))
    roots.extend(_HAMILTONIAN_CANDIDATES)
    tried: List[str] = []
    for root in roots:
        for cand in (
            Path(root) / bench / f"{bench}.pauli",
            Path(root) / f"{bench}.pauli",
        ):
            tried.append(str(cand))
            if cand.is_file():
                return cand.resolve()
    raise BaselineNotFoundError(
        f"hamiltonian Pauli list for {bench!r} not found. Tried:\n  "
        + "\n  ".join(tried)
        + "\nRun: python3 scripts/paulicompress/cli.py prepare --bench "
        + bench
    )


def resolve_after_phase_fold_pauli(
    bench: str,
    *,
    fold_root: Path = AFTER_PHASE_FOLD_DIR,
) -> Path:
    """Locate shipped (or regenerated) after-Phase-Folding ``.pauli``."""
    path = Path(fold_root) / f"{bench}.pauli"
    if path.is_file():
        return path.resolve()
    raise BaselineNotFoundError(
        f"after_phase_fold baseline missing: {path}\n"
        "Expected shipped files under scripts/paulicompress/baselines/after_phase_fold/"
    )


def load_hamiltonian(
    bench: str,
    *,
    bench_root: Optional[Path] = None,
) -> PauliCircuit:
    return load_pauli_file(resolve_hamiltonian_pauli(bench, bench_root=bench_root), name=bench)


def load_after_phase_fold(
    bench: str,
    *,
    fold_root: Path = AFTER_PHASE_FOLD_DIR,
) -> PauliCircuit:
    return load_pauli_file(
        resolve_after_phase_fold_pauli(bench, fold_root=fold_root), name=bench
    )


def load_stage(
    bench: str,
    stage: str,
    *,
    bench_root: Optional[Path] = None,
    fold_root: Path = AFTER_PHASE_FOLD_DIR,
) -> PauliCircuit:
    """``stage`` ∈ {``hamiltonian``, ``after_phase_fold``}."""
    stage = stage.strip().lower().replace("-", "_")
    if stage == "hamiltonian":
        return load_hamiltonian(bench, bench_root=bench_root)
    if stage in {"after_phase_fold", "after_phasefold", "phase_fold", "folded"}:
        return load_after_phase_fold(bench, fold_root=fold_root)
    raise ValueError(
        f"unknown stage {stage!r}; expected 'hamiltonian' or 'after_phase_fold'"
    )


def iter_molecule_benches(
    benches: Optional[Sequence[str]] = None,
) -> Iterable[str]:
    return list(benches) if benches else list(DEFAULT_MOLECULE_BENCHES)
