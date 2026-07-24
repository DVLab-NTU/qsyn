"""Shared helpers for the paulicompress CLI."""

from __future__ import annotations

import math
import os
import re
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, Sequence, Tuple

HERE = Path(__file__).resolve().parent
QSYN_ROOT = HERE.parents[1]
DEFAULT_QSYN_BIN = QSYN_ROOT / "build" / "qsyn"
BENCHMARK_DIR = QSYN_ROOT / "scripts" / "pca_compress" / "benchmark_circuit"


def fidelity_to_l2_budget(f_target: float) -> float:
    """Map process-fidelity target F* → L2 snap budget (small-angle surrogate).

    Matches ``min_pauli_rotation.fidelity_to_l2_budget`` / C++ ``pauli-compress -l``:
    ``B = sqrt(-2 ln F*)`` so that ``sum_j d_j^2 <= B^2`` ≈ ``F >= F*``.
    """
    if f_target <= 0.0 or f_target > 1.0:
        raise ValueError(f"f_target must be in (0, 1], got {f_target}")
    if f_target >= 1.0:
        return 0.0
    return math.sqrt(-2.0 * math.log(f_target))


def run_qsyn_dof(
    lines: Sequence[str],
    qsyn_bin: Path,
    *,
    timeout_s: int = 7200,
    label: str = "qsyn",
) -> Tuple[str, str]:
    qsyn_bin = Path(qsyn_bin)
    if not qsyn_bin.is_file():
        raise FileNotFoundError(
            f"qsyn binary not found: {qsyn_bin}\n"
            "Build first:  cd <qsyn> && make -j$(nproc)"
        )
    dof = "\n".join(lines) + "\nquit -f\n"
    with tempfile.TemporaryDirectory(prefix="paulicompress_") as tmp:
        dof_path = os.path.join(tmp, "run.dof")
        with open(dof_path, "w", encoding="utf-8") as f:
            f.write(dof)
        proc = subprocess.run(
            [str(qsyn_bin), "-q", dof_path],
            cwd=tmp,
            capture_output=True,
            text=True,
            timeout=timeout_s,
            check=False,
        )
        if proc.returncode != 0:
            raise RuntimeError(
                f"qsyn {label} failed (code {proc.returncode})\n"
                f"{proc.stderr[-3000:]}\n{proc.stdout[-1000:]}"
            )
        return proc.stdout, proc.stderr


def parse_qcir_print_stats(stdout: str) -> Dict[str, int]:
    m = re.search(
        r"QCir\s*\(\s*(\d+)\s*qubits,\s*(\d+)\s+gates,\s*(\d+)\s+2-qubits gates,\s*(\d+)\s+T-gates",
        stdout,
    )
    if not m:
        return {}
    return {
        "n_qubits": int(m.group(1)),
        "n_gates": int(m.group(2)),
        "n_2q_gates": int(m.group(3)),
        "n_t_gates": int(m.group(4)),
    }


def write_pauli_list(
    path: Path,
    name: str,
    n_qubits: int,
    rotations: Sequence[Tuple[str, float]],
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"# from-pauli-list input: {name} ({n_qubits} qubits)\n")
        for pauli, theta in rotations:
            f.write(f"{pauli} {theta!r}\n")
