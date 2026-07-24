"""Clifford+T synthesis of the fused NCF unitaries.

Two primitives are used:

* **gridsynth** (``pygridsynth``) synthesizes single-qubit Rz rotations to a
  guaranteed operator distance <= eps.  A general single-qubit unitary is
  decomposed ZYZ into (at most) three Rz rotations plus fixed Cliffords.  Since
  gridsynth is exact-to-eps and deterministic, the resulting *fidelity* is
  synthesizer-independent -- this is why it is a faithful stand-in for Trasyn
  when the quantity of interest is fidelity (the paper's Trasyn only changes the
  T-*count*, not the achievable fidelity at a given eps).

* **Synthetiq** (compiled binary) synthesizes the two-qubit fused unitaries at
  the paper's loose threshold eps=0.12.

Every :class:`SynthResult` carries the concrete Clifford+T gate list (on local
qubit indices), the reconstructed unitary, the achieved process fidelity and the
gate metrics (T-count, T-depth, Clifford count).
"""

from __future__ import annotations

import glob
import math
import os
import re
import subprocess
import tempfile
from dataclasses import dataclass, field
from typing import List, Optional, Sequence, Tuple

import numpy as np

from . import config
from .synthetiq_metric import aligned_fidelity, process_fidelity

# --- gate matrices (for reconstruction) ------------------------------------ #
_H = np.array([[1, 1], [1, -1]], dtype=complex) / math.sqrt(2)
_S = np.array([[1, 0], [0, 1j]], dtype=complex)
_SDG = np.array([[1, 0], [0, -1j]], dtype=complex)
_T = np.array([[1, 0], [0, np.exp(1j * math.pi / 4)]], dtype=complex)
_TDG = np.array([[1, 0], [0, np.exp(-1j * math.pi / 4)]], dtype=complex)
_X = np.array([[0, 1], [1, 0]], dtype=complex)
_Y = np.array([[0, -1j], [1j, 0]], dtype=complex)
_Z = np.array([[1, 0], [0, -1]], dtype=complex)
_GATE1 = {"h": _H, "s": _S, "sdg": _SDG, "t": _T, "tdg": _TDG, "x": _X, "y": _Y, "z": _Z}
_TWO_PI = 2 * math.pi


@dataclass
class SynthResult:
    ops: List[Tuple[str, Tuple[int, ...]]]  # (gate, local-qubit-indices)
    matrix: np.ndarray                      # reconstructed unitary
    t_count: int
    clifford_count: int
    t_depth: int
    fidelity: float                         # aligned (Synthetiq-valid) process F
    eps: float
    fidelity_direct: float = 1.0            # without qubit-permutation alignment
    fidelity_worst: float = 1.0             # min aligned F among Synthetiq candidates
    synthetiq_distance: float = 0.0


# --------------------------------------------------------------------------- #
#  single-qubit synthesis (gridsynth)                                         #
# --------------------------------------------------------------------------- #
_GRID_CACHE = {}


def _grid_letters(theta: float, eps: float) -> str:
    import mpmath
    import pygridsynth

    theta = float(((theta + math.pi) % _TWO_PI) - math.pi)   # wrap to (-pi, pi]
    eps = float(eps)
    key = (round(theta, 12), eps)
    hit = _GRID_CACHE.get(key)
    if hit is not None:
        return hit
    mpmath.mp.dps = max(30, int(-math.log10(eps)) + 20)
    letters = str(pygridsynth.gridsynth_gates(mpmath.mpf(repr(theta)), mpmath.mpf(repr(eps))))
    _GRID_CACHE[key] = letters
    return letters


def _letters_to_ops(letters: str, q: int) -> Tuple[List[Tuple[str, Tuple[int, ...]]], int]:
    ops: List[Tuple[str, Tuple[int, ...]]] = []
    tcount = 0
    for ch in letters:
        if ch == "W":            # global phase e^{i pi/4}, irrelevant to fidelity
            continue
        name = {"H": "h", "S": "s", "T": "t", "X": "x"}[ch]
        ops.append((name, (q,)))
        if name == "t":
            tcount += 1
    return ops, tcount


def _ops_matrix_1q(ops: Sequence[Tuple[str, Tuple[int, ...]]]) -> np.ndarray:
    U = np.eye(2, dtype=complex)
    for name, _ in ops:                       # time order = list order
        U = _GATE1[name] @ U
    return U


def _zyz_angles(U: np.ndarray) -> Tuple[float, float, float]:
    """Return (phi, theta, lam) with U ~ Rz(phi) Ry(theta) Rz(lam) up to phase."""
    det = U[0, 0] * U[1, 1] - U[0, 1] * U[1, 0]
    g = np.sqrt(det + 0j)
    V = U / g
    theta = 2 * math.atan2(abs(V[1, 0]), abs(V[0, 0]))
    a00 = np.angle(V[0, 0])
    a10 = np.angle(V[1, 0])
    if abs(V[0, 0]) < 1e-12:
        # theta = pi: use off-diagonal; fix phi, split via V01/V10
        a01 = np.angle(V[0, 1])
        phi = a10 - a01 + math.pi
        lam = 0.0
    else:
        phi = a10 - a00
        lam = -a10 - a00
    return phi, theta, lam


def synth_1q(U2: np.ndarray, eps: float) -> SynthResult:
    """Synthesize an arbitrary single-qubit unitary on local qubit 0."""
    ops: List[Tuple[str, Tuple[int, ...]]] = []
    tcount = 0
    cliff = 0
    diag = abs(U2[0, 1]) < 1e-9 and abs(U2[1, 0]) < 1e-9

    def add_rz(angle: float):
        nonlocal tcount, cliff
        if abs(((angle + math.pi) % _TWO_PI) - math.pi) < 1e-12:
            return
        letters = _grid_letters(angle, eps)
        rz_ops, tc = _letters_to_ops(letters, 0)
        ops.extend(rz_ops)
        tcount += tc
        cliff += len(rz_ops) - tc

    if diag:
        angle = float(np.angle(U2[1, 1]) - np.angle(U2[0, 0]))
        add_rz(angle)
    else:
        phi, theta, lam = _zyz_angles(U2)
        add_rz(lam)
        # Ry(theta) = S H Rz(theta) H Sdg  (time order: Sdg, H, Rz, H, S)
        for name in ("sdg", "h"):
            ops.append((name, (0,)))
            cliff += 1
        add_rz(theta)
        for name in ("h", "s"):
            ops.append((name, (0,)))
            cliff += 1
        add_rz(phi)

    M = _ops_matrix_1q(ops)
    F = process_fidelity(U2, M)
    return SynthResult(ops, M, tcount, cliff, tcount, F, eps, F, F, 0.0)


# --------------------------------------------------------------------------- #
#  two-qubit synthesis (Synthetiq)                                            #
# --------------------------------------------------------------------------- #
_SYN_QASM_GATE_RE = re.compile(
    r"^\s*(?P<name>[a-z]+)\s+[a-zA-Z_]+\[(?P<a>\d+)\]"
    r"(?:\s*,\s*[a-zA-Z_]+\[(?P<b>\d+)\])?\s*;"
)


def _write_synthetiq_input(path: str, name: str, U: np.ndarray) -> None:
    n = int(round(math.log2(U.shape[0])))
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"{name}\n{n}\n")
        for r in range(U.shape[0]):
            f.write(" ".join(f"({U[r, c].real:.12f},{U[r, c].imag:.12f})" for c in range(U.shape[1])) + "\n")
        for r in range(U.shape[0]):
            f.write(" ".join("1" for _ in range(U.shape[1])) + "\n")


def _parse_synthetiq_qasm(path: str) -> Tuple[List[Tuple[str, Tuple[int, ...]]], int, int]:
    ops: List[Tuple[str, Tuple[int, ...]]] = []
    tcount = 0
    cliff = 0
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith(("OPENQASM", "include", "qreg", "creg", "//", "barrier")):
                continue
            m = _SYN_QASM_GATE_RE.match(line)
            if not m:
                continue
            name = m.group("name")
            qs = [int(m.group("a"))]
            if m.group("b") is not None:
                qs.append(int(m.group("b")))
            ops.append((name, tuple(qs)))
            if name in ("t", "tdg"):
                tcount += 1
            elif name != "id":
                cliff += 1
    return ops, tcount, cliff


def _ops_matrix_2q(ops: Sequence[Tuple[str, Tuple[int, ...]]]) -> np.ndarray:
    U = np.eye(4, dtype=complex)
    cx01 = np.array([[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 1], [0, 0, 1, 0]], dtype=complex)
    cx10 = np.array([[1, 0, 0, 0], [0, 0, 0, 1], [0, 0, 1, 0], [0, 1, 0, 0]], dtype=complex)

    def e1(g, q):
        return np.kron(g, np.eye(2, dtype=complex)) if q == 0 else np.kron(np.eye(2, dtype=complex), g)

    for name, qs in ops:
        if name == "cx":
            G = cx01 if qs == (0, 1) else cx10
        elif name == "cz":
            G = np.diag([1, 1, 1, -1]).astype(complex)
        else:
            G = e1(_GATE1[name], qs[0])
        U = G @ U
    return U


def _t_depth_2q(ops: Sequence[Tuple[str, Tuple[int, ...]]]) -> int:
    """ASAP-scheduled T-depth: number of distinct time layers containing a T."""
    time_q = {0: 0, 1: 0}
    t_layers = set()
    for name, qs in ops:
        tq = max(time_q[q] for q in qs) + 1
        for q in qs:
            time_q[q] = tq
        if name in ("t", "tdg"):
            t_layers.add(tq)
    return len(t_layers)


_SYNTH_CACHE = {}
_FNAME_RE = re.compile(r"[\d.]+-(\d+)-(\d+)-")     # ...-[Tcount]-[Tdepth]-...


def _run_synthetiq(tag: str, inpath: str, outdir: str, eps: float,
                   time_s: float, circuits: int,
                   U_target: Optional[np.ndarray] = None) -> Optional["SynthResult"]:
    for f in glob.glob(os.path.join(outdir, "*.qasm")):
        os.remove(f)
    subprocess.run(
        [config.SYNTHETIQ_BIN, tag + ".txt", "-o", tag,
         "-eps", repr(float(eps)), "-t", str(time_s), "-c", str(circuits)],
        cwd=config.SYNTHETIQ_DIR, capture_output=True, text=True,
        timeout=time_s * circuits + 60,
    )
    best: Optional[SynthResult] = None
    worst_f = 1.0
    for qpath in glob.glob(os.path.join(outdir, "*.qasm")):
        ops, _, cliff = _parse_synthetiq_qasm(qpath)
        m = _FNAME_RE.search(os.path.basename(qpath))
        tcount = int(m.group(1)) if m else sum(1 for g, _ in ops if g in ("t", "tdg"))
        tdepth = int(m.group(2)) if m else _t_depth_2q(ops)
        M = _ops_matrix_2q(ops)
        if U_target is not None:
            f_dir = process_fidelity(U_target, M)
            f_al, f_wi, dist = aligned_fidelity(U_target, M, eps)
            worst_f = min(worst_f, f_wi)
        else:
            f_dir = f_al = f_wi = 0.0
            dist = 0.0
        cand = SynthResult(ops, M, tcount, cliff, tdepth, f_al, eps,
                           f_dir, f_wi, dist)
        if best is None or cand.t_count < best.t_count:
            best = cand
    if best is not None and U_target is not None:
        best.fidelity_worst = worst_f
    return best


def synth_2q(U4: np.ndarray, eps: float, name: str = "grp", time_s: float = 8.0,
             circuits: int = 4) -> SynthResult:
    """Synthesize a two-qubit unitary with Synthetiq at threshold ``eps``.

    Synthetiq only emits circuits within ``eps`` of the target, so we keep the
    lowest-T circuit found.  Synthetiq is stochastic, hence one retry if empty.
    """
    key = (np.round(U4, 10).tobytes(), round(float(eps), 6))
    hit = _SYNTH_CACHE.get(key)
    if hit is not None:
        return hit
    if not os.path.isfile(config.SYNTHETIQ_BIN):
        raise FileNotFoundError(f"synthetiq binary missing: {config.SYNTHETIQ_BIN}")

    indir = os.path.join(config.SYNTHETIQ_DIR, "data", "input")
    outroot = os.path.join(config.SYNTHETIQ_DIR, "data", "output")
    os.makedirs(indir, exist_ok=True)
    tag = f"ncf_{name}_{abs(hash(key)) % (10 ** 8)}"
    inpath = os.path.join(indir, tag + ".txt")
    outdir = os.path.join(outroot, tag)
    os.makedirs(outdir, exist_ok=True)
    _write_synthetiq_input(inpath, tag, U4)

    best = _run_synthetiq(tag, inpath, outdir, eps, time_s, circuits, U4)
    if best is None:                                 # unlucky annealing -> retry
        best = _run_synthetiq(tag, inpath, outdir, eps, time_s * 2, 2, U4)

    try:
        os.remove(inpath)
        for f in glob.glob(os.path.join(outdir, "*.qasm")):
            os.remove(f)
        os.rmdir(outdir)
    except OSError:
        pass

    if best is None:
        best = SynthResult([], np.eye(4, dtype=complex), 0, 0, 0, float("nan"), eps,
                           float("nan"), float("nan"), float("nan"))
    else:
        f_al, f_wi, dist = aligned_fidelity(U4, best.matrix, eps)
        f_dir = process_fidelity(U4, best.matrix)
        best.fidelity = f_al
        best.fidelity_direct = f_dir
        best.fidelity_worst = min(best.fidelity_worst, f_wi)
        best.synthetiq_distance = dist
    _SYNTH_CACHE[key] = best
    return best


# --------------------------------------------------------------------------- #
def _process_fidelity(U_ideal: np.ndarray, U_synth: np.ndarray) -> float:
    return process_fidelity(U_ideal, U_synth)


# --- per-method epsilon (Section V-A4) ------------------------------------- #
def eps_single(n_paulis: int, n_unitaries: int) -> float:
    """Scaled threshold keeping total budget = BASE_EPS * n_paulis constant."""
    return config.BASE_EPS * n_paulis / max(1, n_unitaries)
