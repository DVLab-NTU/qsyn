"""Pauli tableau + Clifford conjugation for the NCF reproduction.

We represent an n-qubit (Hermitian) Pauli string as symplectic bit-vectors
``x`` and ``z`` (Python ints used as bitmasks, bit ``i`` = qubit ``i``) plus a
real ``sign`` in ``{+1, -1}``.  The single-qubit operator on qubit ``i`` is::

    (x_i, z_i) = (0,0)->I  (1,0)->X  (0,1)->Z  (1,1)->Y

Clifford conjugation ``P -> C P C^dag`` maps Hermitian Paulis to Hermitian
Paulis (sign stays real).  To avoid sign-bookkeeping bugs we *compute* the
per-gate conjugation tables numerically at import time from explicit matrices.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Sequence, Tuple

import numpy as np

# --- single-qubit Pauli matrices ------------------------------------------- #
_I = np.eye(2, dtype=complex)
_X = np.array([[0, 1], [1, 0]], dtype=complex)
_Y = np.array([[0, -1j], [1j, 0]], dtype=complex)
_Z = np.array([[1, 0], [0, -1]], dtype=complex)
_MAT = {"I": _I, "X": _X, "Y": _Y, "Z": _Z}
_BITS = {"I": (0, 0), "X": (1, 0), "Z": (0, 1), "Y": (1, 1)}
_FROM_BITS = {(0, 0): "I", (1, 0): "X", (0, 1): "Z", (1, 1): "Y"}

_H = np.array([[1, 1], [1, -1]], dtype=complex) / np.sqrt(2)
_S = np.array([[1, 0], [0, 1j]], dtype=complex)
_CX = np.array(
    [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 1], [0, 0, 1, 0]], dtype=complex
)


def _match_signed(mat: np.ndarray, basis: Dict[str, np.ndarray]) -> Tuple[str, int]:
    for label, ref in basis.items():
        if np.allclose(mat, ref, atol=1e-9):
            return label, 1
        if np.allclose(mat, -ref, atol=1e-9):
            return label, -1
    raise ValueError("conjugation did not map to a signed Pauli")


def _build_1q_table(gate: np.ndarray) -> Dict[str, Tuple[str, int]]:
    tab = {}
    for p, m in _MAT.items():
        tab[p] = _match_signed(gate @ m @ gate.conj().T, _MAT)
    return tab


def _build_cx_table() -> Dict[Tuple[str, str], Tuple[str, str, int]]:
    basis2 = {
        (a, b): np.kron(_MAT[a], _MAT[b]) for a in _MAT for b in _MAT
    }
    tab = {}
    for a in _MAT:
        for b in _MAT:
            conj = _CX @ np.kron(_MAT[a], _MAT[b]) @ _CX.conj().T
            for (ca, cb), ref in basis2.items():
                if np.allclose(conj, ref, atol=1e-9):
                    tab[(a, b)] = (ca, cb, 1)
                    break
                if np.allclose(conj, -ref, atol=1e-9):
                    tab[(a, b)] = (ca, cb, -1)
                    break
            else:  # pragma: no cover
                raise ValueError(f"CX conj failed for {a}{b}")
    return tab


CONJ_H = _build_1q_table(_H)
CONJ_S = _build_1q_table(_S)
CONJ_CX = _build_cx_table()


# --------------------------------------------------------------------------- #
@dataclass
class SignedPauli:
    """Hermitian Pauli string: sign * (tensor of I/X/Y/Z), symplectic (x,z)."""

    n: int
    x: int = 0
    z: int = 0
    sign: int = 1

    # --- constructors ------------------------------------------------------ #
    @classmethod
    def from_string(cls, s: str, sign: int = 1) -> "SignedPauli":
        n = len(s)
        x = z = 0
        for i, ch in enumerate(s):
            xi, zi = _BITS[ch]
            if xi:
                x |= 1 << i
            if zi:
                z |= 1 << i
        return cls(n, x, z, sign)

    def to_string(self) -> str:
        out = []
        for i in range(self.n):
            xi = (self.x >> i) & 1
            zi = (self.z >> i) & 1
            out.append(_FROM_BITS[(xi, zi)])
        return "".join(out)

    def op_at(self, i: int) -> str:
        return _FROM_BITS[((self.x >> i) & 1, (self.z >> i) & 1)]

    def copy(self) -> "SignedPauli":
        return SignedPauli(self.n, self.x, self.z, self.sign)

    # --- structure --------------------------------------------------------- #
    def support(self) -> List[int]:
        return [i for i in range(self.n) if (self.x >> i) & 1 or (self.z >> i) & 1]

    def weight(self) -> int:
        return len(self.support())

    def key(self) -> Tuple[int, int]:
        """Symplectic identity (ignores sign) for membership tests."""
        return (self.x, self.z)

    # --- symplectic algebra ------------------------------------------------ #
    def commutes_with(self, other: "SignedPauli") -> bool:
        v = (self.x & other.z) ^ (self.z & other.x)
        return bin(v).count("1") % 2 == 0

    def xor_support(self, other: "SignedPauli") -> Tuple[int, int]:
        """Symplectic pattern of the product (ignores phase/sign)."""
        return (self.x ^ other.x, self.z ^ other.z)

    # --- Clifford conjugation (in place): P -> C P C^dag ------------------- #
    def _apply_1q(self, table: Dict[str, Tuple[str, int]], i: int) -> None:
        op = self.op_at(i)
        new, s = table[op]
        self.sign *= s
        xi, zi = _BITS[new]
        self.x = (self.x & ~(1 << i)) | (xi << i)
        self.z = (self.z & ~(1 << i)) | (zi << i)

    def apply_h(self, i: int) -> None:
        self._apply_1q(CONJ_H, i)

    def apply_s(self, i: int) -> None:
        self._apply_1q(CONJ_S, i)

    def apply_cx(self, c: int, t: int) -> None:
        oc, ot = self.op_at(c), self.op_at(t)
        nc, nt, s = CONJ_CX[(oc, ot)]
        self.sign *= s
        for i, new in ((c, nc), (t, nt)):
            xi, zi = _BITS[new]
            self.x = (self.x & ~(1 << i)) | (xi << i)
            self.z = (self.z & ~(1 << i)) | (zi << i)

    def to_matrix_on(self, qubits: Sequence[int]) -> np.ndarray:
        """Dense matrix of this (signed) Pauli restricted to ``qubits`` order.

        Assumes the Pauli acts trivially outside ``qubits``.  ``qubits[0]`` is
        the most-significant tensor factor.
        """
        mat = np.array([[self.sign]], dtype=complex)
        for q in qubits:
            mat = np.kron(mat, _MAT[self.op_at(q)])
        return mat


# --------------------------------------------------------------------------- #
#  GF(2) linear algebra for generator / generated partition                   #
# --------------------------------------------------------------------------- #
def _vec(p: SignedPauli) -> int:
    """2n-bit vector (x bits then z bits) for GF(2) row reduction."""
    return p.x | (p.z << p.n)


def find_generators(paulis: Sequence[SignedPauli]) -> Tuple[List[int], List[int]]:
    """Partition indices into (generator_indices, generated_indices).

    Generators are a maximal GF(2)-independent subset (greedy, order-preserving);
    the remaining strings are products of chosen generators.
    """
    basis: List[Tuple[int, int]] = []  # (pivot_bit, reduced_vector)
    gen: List[int] = []
    gend: List[int] = []
    for idx, p in enumerate(paulis):
        v = _vec(p)
        r = v
        for pivot, bvec in basis:
            if (r >> pivot) & 1:
                r ^= bvec
        if r == 0:
            gend.append(idx)
        else:
            pivot = r.bit_length() - 1
            basis.append((pivot, r))
            gen.append(idx)
    return gen, gend
