"""Minimal Pauli-list model for thesis zero-sweep (F-cost heuristic).

Subset of ``scripts/pca_compress/compress_ncf.py``: Rotation / PauliCircuit,
lossless D-merge + F-cancel, and the coefficient fidelity surrogate used in
Chapter 2 / Chapter 4 of the thesis.
"""

from __future__ import annotations

import math
import re
from collections import OrderedDict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

TWO_PI = 2.0 * math.pi
HALF_PI = math.pi / 2.0
_HEADER_QUBITS_RE = re.compile(r"\((\d+)\s*qubits?\)", re.IGNORECASE)


@dataclass
class Rotation:
    """A single Pauli rotation ``exp(-i * theta/2 * P)`` (θ stored as list angle)."""

    pauli: str
    theta: float


@dataclass
class PauliCircuit:
    name: str
    n_qubits: int
    rotations: List[Rotation]

    def copy(self) -> "PauliCircuit":
        return PauliCircuit(
            self.name,
            self.n_qubits,
            [Rotation(r.pauli, r.theta) for r in self.rotations],
        )

    @property
    def n_rot(self) -> int:
        return len(self.rotations)


def _wrap(theta: float) -> float:
    return (theta + math.pi) % TWO_PI - math.pi


def _aggregate(rots: Sequence[Rotation]) -> "OrderedDict[str, float]":
    agg: "OrderedDict[str, float]" = OrderedDict()
    for r in rots:
        agg[r.pauli] = agg.get(r.pauli, 0.0) + r.theta
    return agg


def method_D_merge(pc: PauliCircuit) -> PauliCircuit:
    """Lossless: merge consecutive / identical Pauli strings (sum angles)."""
    agg = _aggregate(pc.rotations)
    return PauliCircuit(pc.name, pc.n_qubits, [Rotation(p, th) for p, th in agg.items()])


def method_F_cancel(pc: PauliCircuit, tol: float = 1e-9) -> PauliCircuit:
    """Lossless: drop 2π-period (≈ identity) terms after merge."""
    agg = _aggregate(pc.rotations)
    rots: List[Rotation] = []
    for p, th in agg.items():
        r = th % TWO_PI
        if min(r, TWO_PI - r) <= tol:
            continue
        rots.append(Rotation(p, th))
    return PauliCircuit(pc.name, pc.n_qubits, rots)


def coeff_error(
    original: Sequence[Rotation],
    compressed: Sequence[Rotation],
) -> Tuple[float, float, float]:
    """Return ``(l1, l2, F_approx)`` with ``F = prod_P |cos(d_P)|``, ``d=(θ-θ')/2``."""
    a = _aggregate(original)
    b = _aggregate(compressed)
    keys = set(a) | set(b)
    l1 = 0.0
    l2sq = 0.0
    log_fid = 0.0
    fid_is_zero = False
    for p in keys:
        d = (a.get(p, 0.0) - b.get(p, 0.0)) / 2.0
        d = _wrap(d)
        if d == 0.0:
            continue
        l1 += abs(d)
        l2sq += d * d
        c = abs(math.cos(d))
        if c < 1e-300:
            fid_is_zero = True
        else:
            log_fid += math.log(c)
    fid = 0.0 if fid_is_zero else math.exp(log_fid)
    return l1, math.sqrt(l2sq), fid


def _l2_error_if_truncate_at_eps(pc: PauliCircuit, eps: float) -> float:
    merged = method_D_merge(pc)
    kept = [r for r in merged.rotations if abs(_wrap(r.theta)) >= eps]
    _, l2, _ = coeff_error(merged.rotations, kept)
    return l2


def max_eps_for_l2_budget(pc: PauliCircuit, l2_budget: float) -> Tuple[float, float]:
    """Largest truncate threshold with truncate-L2 ≤ budget (equiv. Method A)."""
    if l2_budget <= 1e-15:
        return 0.0, 0.0
    merged = method_D_merge(pc)
    if not merged.rotations:
        return 0.0, 0.0
    hi = max(abs(_wrap(r.theta)) for r in merged.rotations) + 1e-15
    if _l2_error_if_truncate_at_eps(pc, hi) <= l2_budget:
        return hi, _l2_error_if_truncate_at_eps(pc, hi)
    lo = 0.0
    for _ in range(48):
        mid = (lo + hi) / 2.0
        if _l2_error_if_truncate_at_eps(pc, mid) <= l2_budget:
            lo = mid
        else:
            hi = mid
    return lo, _l2_error_if_truncate_at_eps(pc, lo)


def load_pauli_file(path: Path, *, name: Optional[str] = None) -> PauliCircuit:
    path = Path(path)
    rotations: List[Rotation] = []
    n_qubits: Optional[int] = None
    title = name or path.stem
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("#"):
            m = _HEADER_QUBITS_RE.search(line)
            if m:
                n_qubits = int(m.group(1))
            continue
        parts = line.split()
        if len(parts) < 2:
            raise ValueError(f"{path}: bad line {raw!r}")
        pauli, theta_s = parts[0].upper(), parts[1]
        rotations.append(Rotation(pauli, float(theta_s)))
        if n_qubits is None:
            n_qubits = len(pauli)
        elif len(pauli) != n_qubits:
            raise ValueError(f"{path}: length mismatch on {pauli}")
    if n_qubits is None:
        raise ValueError(f"{path}: empty Pauli list")
    return PauliCircuit(title, n_qubits, rotations)


def write_pauli_file(pc: PauliCircuit, path: Path) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"# from-pauli-list input: {pc.name} ({pc.n_qubits} qubits)\n")
        for r in pc.rotations:
            f.write(f"{r.pauli} {r.theta!r}\n")
