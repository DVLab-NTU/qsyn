"""Fuse a reduced NCF group into a small (1- or 2-qubit) target unitary.

After :func:`clifford_gen.reduce_group` the group's Pauli rotations act only on
the ``pivots`` qubits.  Their ordered product

    U = prod_k exp(-i theta_k / 2 * P'_k)

is a ``2^|pivots| x 2^|pivots|`` unitary that we hand to a synthesizer.  The
Clifford wrapper is emitted separately (see :mod:`synth`)::

    original sub-product  ==  C^dag  (fused U on pivots)  C
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import List, Sequence, Tuple

import numpy as np
from scipy.linalg import expm

from .clifford_gen import Gate, reduce_group
from .grouping import Group
from .tableau import SignedPauli


@dataclass
class FusedGroup:
    pivots: List[int]                 # global qubit indices the unitary acts on
    clifford: List[Gate]              # C : conjugation that isolates the group
    unitary: np.ndarray               # fused target on the pivots (2^k x 2^k)
    conj_rotations: List[Tuple[SignedPauli, float]]
    n_pauli: int                      # number of original rotations fused


def fused_unitary(conj_rots: Sequence[Tuple[SignedPauli, float]], pivots: Sequence[int]) -> np.ndarray:
    dim = 1 << len(pivots)
    U = np.eye(dim, dtype=complex)
    for sp, theta in conj_rots:
        P = sp.to_matrix_on(pivots)
        U = expm(-0.5j * theta * P) @ U
    return U


def fuse(group: Group) -> FusedGroup:
    gates, pivots, conj = reduce_group(group.paulis)
    conj_rots = list(zip(conj, group.thetas))
    U = fused_unitary(conj_rots, pivots)
    return FusedGroup(
        pivots=list(pivots),
        clifford=gates,
        unitary=U,
        conj_rotations=conj_rots,
        n_pauli=group.size,
    )
