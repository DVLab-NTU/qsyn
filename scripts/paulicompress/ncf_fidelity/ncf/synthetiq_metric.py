"""Synthetiq distance / qubit-permutation alignment (ExactEqualityComputer).

Synthetiq accepts circuits that match the target up to qubit permutation (and
optionally inverse/adjoint).  Comparing ``|Tr(U† V)|/d`` without alignment
under-estimates fidelity for some groups (notably H2O / N2 ncf2).
"""

from __future__ import annotations

import itertools
import math
from typing import Iterable, Tuple

import numpy as np


def permute_qubits(U: np.ndarray, perm: Tuple[int, ...]) -> np.ndarray:
    """Relabel qubit order: ``|b0 b1 ...⟩ → |b_perm[0] b_perm[1] ...⟩``."""
    nq = int(round(math.log2(U.shape[0])))
    out = np.zeros_like(U)
    for i in range(U.shape[0]):
        for j in range(U.shape[1]):
            bi = [(i >> (nq - 1 - k)) & 1 for k in range(nq)]
            bj = [(j >> (nq - 1 - k)) & 1 for k in range(nq)]
            ni = sum(bi[perm[k]] << (nq - 1 - k) for k in range(nq))
            nj = sum(bj[perm[k]] << (nq - 1 - k) for k in range(nq))
            out[ni, nj] = U[i, j]
    return out


def synthetiq_distance(U_target: np.ndarray, U_circ: np.ndarray) -> float:
    """``ExactEqualityComputer`` distance (cost.cpp, full cover)."""
    n = U_target.shape[0]
    norm_cst = float(n * n)
    circ_size = float(np.sum(np.abs(U_circ) ** 2))
    sq = float(np.sum(np.abs(U_target) ** 2))
    conj = np.vdot(U_target, U_circ)
    num = max(0.0, sq + circ_size - 2.0 * abs(conj))
    return (1.0 / math.sqrt(2.0)) * math.sqrt(num) / (norm_cst ** 0.25)


def process_fidelity(U_ideal: np.ndarray, U_synth: np.ndarray) -> float:
    d = U_ideal.shape[0]
    return float(abs(np.trace(U_ideal.conj().T @ U_synth)) / d)


def _target_variants(U: np.ndarray, use_inverse: bool = True) -> Iterable[np.ndarray]:
    nq = int(round(math.log2(U.shape[0])))
    for perm in itertools.permutations(range(nq)):
        Up = permute_qubits(U, perm)
        yield Up
        if use_inverse:
            yield Up.conj().T


def aligned_fidelity(
    U_ideal: np.ndarray, U_synth: np.ndarray, eps: float, *, use_inverse: bool = True
) -> Tuple[float, float, float]:
    """Return (best_F, worst_F_within_eps, best_distance) over Synthetiq variants."""
    best_f, worst_f, best_d = 0.0, 1.0, float("inf")
    for M in _target_variants(U_ideal, use_inverse=use_inverse):
        d = synthetiq_distance(M, U_synth)
        f = process_fidelity(M, U_synth)
        if d < best_d:
            best_d = d
        if d <= eps + 1e-6:
            best_f = max(best_f, f)
            worst_f = min(worst_f, f)
    if best_f <= 0.0:
        best_f = process_fidelity(U_ideal, U_synth)
        worst_f = best_f
    return best_f, worst_f, best_d
