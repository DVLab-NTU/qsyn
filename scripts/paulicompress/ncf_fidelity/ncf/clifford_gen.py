"""Clifford circuit generation for an NCF group (Section IV-B of the paper).

Given the Pauli strings of a group, we build a Clifford circuit ``C`` (a list of
H/S/CX gates) such that conjugation ``P -> C P C^dag`` maps every Pauli in the
group's subgroup to act non-trivially on only the ``pivot`` qubits.  The number
of pivots equals the number of hyperbolic (anticommuting) pairs plus isotropic
generators in the subgroup:

  * an anticommuting pair -> one qubit (X_p, Z_p),
  * an isolated commuting generator -> one qubit (Z_p).

We first put the generator basis into symplectic canonical form (Gram-Schmidt:
anticommuting pairs that mutually commute across pairs).  This guarantees that
once a pivot is fixed, every remaining generator is trivial on it, so later
reduction steps never disturb an already-fixed pivot.

A ``Gate`` is a tuple ``(name, (qubits...))`` with ``name`` in
``{"h", "s", "cx"}``.
"""

from __future__ import annotations

from typing import List, Sequence, Tuple

from .tableau import SignedPauli, find_generators

Gate = Tuple[str, Tuple[int, ...]]


def _sp(x1: int, z1: int, x2: int, z2: int) -> int:
    v = (x1 & z2) ^ (z1 & x2)
    return bin(v).count("1") & 1


def symplectic_gram_schmidt(vectors: Sequence[Tuple[int, int]]):
    """Return (pairs, singles) of (x,z) vectors spanning the same subgroup.

    ``pairs`` are (a, b) anticommuting; ``singles`` are isotropic.  Every pair
    / single mutually commutes with all others.
    """
    V = [list(v) for v in vectors]
    used = [False] * len(V)
    pairs: List[Tuple[Tuple[int, int], Tuple[int, int]]] = []
    singles: List[Tuple[int, int]] = []
    for i in range(len(V)):
        if used[i]:
            continue
        xi, zi = V[i]
        if xi == 0 and zi == 0:
            used[i] = True
            continue
        # find an anticommuting partner among the unused
        j = -1
        for k in range(i + 1, len(V)):
            if used[k]:
                continue
            xk, zk = V[k]
            if xk == 0 and zk == 0:
                continue
            if _sp(xi, zi, xk, zk):
                j = k
                break
        if j == -1:
            used[i] = True
            singles.append((xi, zi))
            continue
        xj, zj = V[j]
        used[i] = used[j] = True
        # decouple all remaining unused vectors from this pair
        for k in range(len(V)):
            if used[k]:
                continue
            xk, zk = V[k]
            if _sp(xk, zk, xj, zj):      # anticommutes with b -> multiply by a
                xk ^= xi
                zk ^= zi
            if _sp(xk, zk, xi, zi):      # anticommutes with a -> multiply by b
                xk ^= xj
                zk ^= zj
            V[k] = [xk, zk]
        pairs.append(((xi, zi), (xj, zj)))
    return pairs, singles


class _Reducer:
    """Applies a recorded Clifford to a live working set of SignedPaulis.

    All generators are kept live (in the *current* conjugated frame) so that
    later reduction steps see the effect of earlier ones.
    """

    def __init__(self, n: int, work: List[SignedPauli]):
        self.n = n
        self.gates: List[Gate] = []
        self.work = work

    def _apply(self, name: str, qs: Tuple[int, ...]) -> None:
        self.gates.append((name, qs))
        for p in self.work:
            if name == "h":
                p.apply_h(qs[0])
            elif name == "s":
                p.apply_s(qs[0])
            elif name == "cx":
                p.apply_cx(qs[0], qs[1])

    def reduce_single(self, gi: int, pivots: set) -> int:
        """Reduce work[gi] (isotropic) to Z on a fresh pivot; return pivot."""
        g = self.work[gi]
        for q in list(g.support()):
            if q in pivots:
                continue
            op = g.op_at(q)
            if op == "Z":
                self._apply("h", (q,))
            elif op == "Y":
                self._apply("s", (q,))
        supp = [q for q in g.support() if q not in pivots]
        p = supp[0]
        for q in supp:
            if q != p and (g.x >> q) & 1:
                self._apply("cx", (p, q))
        self._apply("h", (p,))            # X_p -> Z_p
        return p

    def reduce_pair(self, ia: int, ib: int, pivots: set) -> int:
        """Reduce (work[ia], work[ib]) to (X_p, Z_p) on a fresh pivot; return p."""
        pa = self.work[ia]
        pb = self.work[ib]
        # 1. a -> X_p (clear a's Z on non-pivot qubits, then collapse X to pivot)
        for q in list(pa.support()):
            if q in pivots:
                continue
            op = pa.op_at(q)
            if op == "Z":
                self._apply("h", (q,))
            elif op == "Y":
                self._apply("s", (q,))
        supp = [q for q in pa.support() if q not in pivots]
        p = supp[0]
        for q in supp:
            if q != p and (pa.x >> q) & 1:
                self._apply("cx", (p, q))
        # 2. swap a to Z_p so reshaping b (now carrying x_p) is safe
        self._apply("h", (p,))
        # 3. b -> all-X (S on p only turns Y->X, safe for a=Z_p)
        for q in list(pb.support()):
            if q in pivots and q != p:
                continue
            op = pb.op_at(q)
            if op == "Z":
                self._apply("h", (q,))
            elif op == "Y":
                self._apply("s", (q,))
        # 4. collapse b's X-support onto p (CX(p,q) keeps a=Z_p invariant)
        for q in [q for q in pb.support() if q != p and (pb.x >> q) & 1]:
            self._apply("cx", (p, q))
        # 5. swap back: a -> X_p, b -> Z_p
        self._apply("h", (p,))
        return p


def reduce_group(group_paulis: Sequence[SignedPauli]):
    """Reduce a group's subgroup to its pivot qubits.

    Returns ``(gates, pivots, conjugated)`` where ``gates`` is the Clifford
    (list of Gate), ``pivots`` the list of pivot qubits, and ``conjugated`` the
    input ``group_paulis`` after conjugation (each supported on ``pivots``).
    """
    n = group_paulis[0].n
    gen_idx, _ = find_generators(group_paulis)
    gen_vecs = [(group_paulis[i].x, group_paulis[i].z) for i in gen_idx]
    pairs, singles = symplectic_gram_schmidt(gen_vecs)

    # live working set: pair generators first (flattened), then singles
    work: List[SignedPauli] = []
    pair_slots: List[Tuple[int, int]] = []
    for a, b in pairs:
        ia, ib = len(work), len(work) + 1
        work.append(SignedPauli(n, a[0], a[1], 1))
        work.append(SignedPauli(n, b[0], b[1], 1))
        pair_slots.append((ia, ib))
    single_slots: List[int] = []
    for v in singles:
        single_slots.append(len(work))
        work.append(SignedPauli(n, v[0], v[1], 1))

    red = _Reducer(n, work)
    pivots: List[int] = []
    for ia, ib in pair_slots:
        pivots.append(red.reduce_pair(ia, ib, set(pivots)))
    for gi in single_slots:
        pivots.append(red.reduce_single(gi, set(pivots)))

    # apply the assembled Clifford to the actual group Paulis
    conj = [p.copy() for p in group_paulis]
    for name, qs in red.gates:
        for p in conj:
            if name == "h":
                p.apply_h(qs[0])
            elif name == "s":
                p.apply_s(qs[0])
            elif name == "cx":
                p.apply_cx(qs[0], qs[1])
    return red.gates, sorted(pivots), conj


def num_pivots(group_paulis: Sequence[SignedPauli]) -> int:
    """How many qubits the group's subgroup reduces to (without building C)."""
    gen_idx, _ = find_generators(group_paulis)
    gen_vecs = [(group_paulis[i].x, group_paulis[i].z) for i in gen_idx]
    pairs, singles = symplectic_gram_schmidt(gen_vecs)
    return len(pairs) + len(singles)
