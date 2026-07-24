"""Paths and constants for NCF (arXiv:2510.13573) synthesis-fidelity audit.

Packaged under ``scripts/paulicompress/ncf_fidelity/``.  Default Pauli inputs
come from the Proposed Flow reproduction bundle (same Table-IV counts).
Synthetiq is optional and resolved via ``NCF_SYNTHETIQ_DIR`` / legacy path.
"""

from __future__ import annotations

import os
from pathlib import Path

# --- directory layout ------------------------------------------------------ #
HERE = Path(__file__).resolve().parent                 # .../ncf_fidelity/ncf
NCF_FIDELITY_DIR = HERE.parent                         # .../ncf_fidelity
PAULICOMPRESS_DIR = NCF_FIDELITY_DIR.parent             # .../paulicompress
QSYN_ROOT = PAULICOMPRESS_DIR.parents[1]               # .../qsyn
PCA_COMPRESS_DIR = QSYN_ROOT / "scripts" / "pca_compress"

CIRCUITS_DIR = str(NCF_FIDELITY_DIR / "circuits")
RESULTS_DIR = str(NCF_FIDELITY_DIR / "results")
TOOLS_DIR = str(NCF_FIDELITY_DIR / "tools")

# Default Hamiltonian Pauli lists (Table IV inputs).
BUNDLE_ORIGINAL = (
    PCA_COMPRESS_DIR
    / "reproduction_bundle_proposed_flow"
    / "01_original_benchmarks"
)

# Optional: legacy track_a caches (local experiment farm; may be absent).
PIPELINE_V8_DIR = str(
    PCA_COMPRESS_DIR / "results_pc_axis" / "pipeline_v8"
)
TRACK_A_CIRCUITS = os.path.join(PIPELINE_V8_DIR, "track_a", "circuits")

# --- Synthetiq binary (2-qubit pathway only) -------------------------------- #
# Prefer env, then vendored tools/, then the local ncf_repro checkout.
_LEGACY_SYNTHETIQ = (
    PCA_COMPRESS_DIR
    / "results_pc_axis"
    / "pipeline_v8"
    / "ncf_repro"
    / "tools"
    / "synthetiq"
)


def _resolve_synthetiq_dir() -> str:
    env = os.environ.get("NCF_SYNTHETIQ_DIR", "").strip()
    if env:
        return env
    local = NCF_FIDELITY_DIR / "tools" / "synthetiq"
    if (local / "bin" / "main").is_file():
        return str(local)
    if (_LEGACY_SYNTHETIQ / "bin" / "main").is_file():
        return str(_LEGACY_SYNTHETIQ)
    return str(local)


SYNTHETIQ_DIR = _resolve_synthetiq_dir()
SYNTHETIQ_BIN = os.path.join(SYNTHETIQ_DIR, "bin", "main")

# --- benchmark table (Table IV of the paper) ------------------------------- #
BENCHMARK_TABLE = {
    "LiH": (12, 630),
    "H2O": (14, 1085),
    "N2": (20, 2950),
    "H2S": (22, 6245),
    "CO2": (30, 16121),
    "Ising-2D-30": (30, 79),
    "Ising-2D-60": (60, 164),
    "Ising-3D-30": (30, 89),
    "Ising-3D-60": (60, 193),
    "Heisenberg-2D-30": (30, 147),
    "Heisenberg-2D-60": (60, 312),
    "Heisenberg-3D-30": (30, 177),
    "Heisenberg-3D-60": (60, 399),
}

TABLE_V_BENCHMARKS = [
    "LiH",
    "H2O",
    "N2",
    "Ising-2D-30",
    "Ising-2D-60",
    "Ising-3D-30",
    "Ising-3D-60",
    "Heisenberg-2D-30",
    "Heisenberg-2D-60",
    "Heisenberg-3D-30",
    "Heisenberg-3D-60",
]

PAPER_TABLE_V = {
    "LiH": ((1419, 1352, 12774), (1397, 757, 8890), (818, 716, 5767), (49, 42, 5289)),
    "H2O": ((3746, 3415, 27017), (3662, 2412, 14111), (1262, 1035, 14434), (375, 303, 11691)),
    "N2": ((7537, 7431, 82196), (7138, 4237, 52431), (3578, 3101, 45955), (541, 516, 41575)),
    "Ising-2D-30": ((790, 90, 1899), (790, 90, 1270), (223, 20, 593), (97, 16, 533)),
    "Ising-2D-60": ((1640, 290, 3924), (1640, 160, 2664), (488, 20, 1363), (200, 16, 1100)),
    "Ising-3D-30": ((890, 220, 2109), (890, 90, 1765), (293, 29, 680), (90, 21, 595)),
    "Ising-3D-60": ((1930, 280, 4533), (1930, 190, 3234), (691, 26, 1498), (195, 20, 1327)),
    "Heisenberg-2D-30": ((2352, 864, 4570), (2352, 556, 4531), (521, 87, 1222), (224, 52, 1117)),
    "Heisenberg-2D-60": ((4993, 1345, 9725), (4972, 736, 9665), (1131, 94, 2679), (493, 80, 2479)),
    "Heisenberg-3D-30": ((2832, 1008, 5831), (2832, 720, 5465), (652, 108, 1523), (253, 68, 1378)),
    "Heisenberg-3D-60": ((6384, 1872, 16449), (6382, 1296, 12344), (1481, 115, 3472), (640, 107, 3234)),
}

BASE_EPS = 0.001
SYNTHETIQ_EPS = 0.12
WINDOW_SINGLE = 4
WINDOW_TWO = 128
DT = 0.05

# Override via CLI / env for custom prepare() output.
_DEFAULT_BENCH_ROOT = os.environ.get("NCF_BENCH_ROOT", "").strip()


def set_bench_root(path: str | None) -> None:
    global _DEFAULT_BENCH_ROOT
    _DEFAULT_BENCH_ROOT = (path or "").strip()


def resolve_pauli_path(bench: str) -> str:
    """Resolve ``.pauli`` for ``bench`` (bench-root → bundle → track_a)."""
    name = bench
    candidates = []
    if _DEFAULT_BENCH_ROOT:
        root = Path(_DEFAULT_BENCH_ROOT)
        candidates.append(root / name / f"{name}.pauli")
        candidates.append(root / f"{name}.pauli")
    candidates.append(BUNDLE_ORIGINAL / name / f"{name}.pauli")
    candidates.append(Path(TRACK_A_CIRCUITS) / f"{name}_fast_df" / "00_cpf.pauli")
    for p in candidates:
        if p.is_file():
            return str(p)
    tried = "\n  ".join(str(p) for p in candidates)
    raise FileNotFoundError(
        f"raw Pauli list not found for {name}. Tried:\n  {tried}\n"
        "Run: python3 scripts/paulicompress/cli.py prepare --bench "
        f"{name} --out out/01_original_benchmarks\n"
        "Or set NCF_BENCH_ROOT / --bench-root."
    )


def track_a_raw_pauli(bench: str) -> str:
    """Backward-compatible alias used by fidelity helpers."""
    return resolve_pauli_path(bench)
