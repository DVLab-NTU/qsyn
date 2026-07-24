"""Generate one-Trotter-step OpenQASM 2.0 circuits for the NCF Table IV benchmarks.

Reference: Yingheng Li, Xulong Tang, Paul Hovland, Ji Liu,
"Non-Clifford Fusion: T-Gate Optimization for Quantum Simulation",
arXiv:2510.13573 (2025), Table IV.

This script can produce two families of benchmarks:

1. Lattice (Ising / Heisenberg, 2D / 3D, 30 / 60 qubits)
   - Ising (TFIM):  H = -J sum_<ij> Z_i Z_j - h sum_i X_i
     with spatially varying J_ij, h_i (deterministic smooth modulation).
   - Heisenberg (XXZ): H = sum_<ij> (J_xy (X_i X_j + Y_i Y_j) + J_z Z_i Z_j)
     with per-edge anisotropy.
   Default Trotter step dt=0.05 (continuous angles via 2*w_j*dt).
   Pauli-string *counts* still match NCF Table IV; only coefficients differ
   from the old uniform-coeff=1 toy model.

2. Molecule (LiH / H2O / N2 / H2S / CO2)
   - All emitted from PySCF Hamiltonian coefficients (non-uniform angles):
     * LiH:  `chemistry/hamlib/HF-BK12.json` (Hamlib / PySCF, 12 q, 630 terms)
     * H2O, N2, H2S, CO2: `source/paulihedral_data/<name>.pickle`
       (PySCF + Bravyi-Kitaev, Paulihedral / Phoenix)

Each Pauli rotation `exp(-i w P)` is emitted as the standard
"basis-change + CNOT-tree + Rz + reverse" circuit fragment.

Usage:
    python gen_benchmark_qasm.py --all                # generate all 13 benchmarks
    python gen_benchmark_qasm.py --lattice            # only lattice circuits
    python gen_benchmark_qasm.py --molecule H2O N2    # only specific molecules

The output QASM files live in ./lattice/ and ./molecule/ respectively.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import pickle
import sys
from dataclasses import dataclass
from typing import Iterable, List, Optional, Sequence, Tuple

HERE = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(HERE, "source")
PH_DATA_DIR = os.path.join(SRC_DIR, "paulihedral_data")

if SRC_DIR not in sys.path:
    sys.path.insert(0, SRC_DIR)
HAMLIB_DIR = os.path.join(HERE, "chemistry", "hamlib")
LATTICE_OUT = os.path.join(HERE, "lattice")
MOLECULE_OUT = os.path.join(HERE, "molecule")


# --------------------------------------------------------------------------- #
#  Lattice physical parameters (spatially varying, reproducible)              #
# --------------------------------------------------------------------------- #

@dataclass(frozen=True)
class LatticePhysics:
    """Coefficients for one Trotter step  exp(-i H dt).

    Each Pauli term in the emitted QASM uses  rz(2 * w_j * dt).
    Edge / site weights are deterministic smooth functions of indices
    (fixed seed logic, not Monte Carlo) so benchmarks stay reproducible
    while angles are continuously non-uniform like real lattice models.
    """
    dt: float = 0.05
    J0: float = 0.52          # Ising ZZ coupling scale
    h0: float = 1.15          # Ising transverse field scale
    J_xy: float = 0.48        # Heisenberg XX / YY scale
    J_z: float = 0.56         # Heisenberg ZZ scale (anisotropy vs J_xy)
    edge_amp: float = 0.28    # relative edge-to-edge variation
    field_amp: float = 0.20   # relative site field variation
    aniso_amp: float = 0.15   # extra YY vs XX split on an edge


def make_lattice_physics(dt: float = 0.05, **kwargs) -> LatticePhysics:
    return LatticePhysics(dt=dt, **kwargs)


def _edge_coeff(a: int, b: int, nq: int, base: float, amp: float) -> float:
    lo, hi = (a, b) if a <= b else (b, a)
    phase = lo * 0.731 + hi * 1.173 + nq * 0.091
    return base * (1.0 + amp * math.sin(phase)
                   + 0.5 * amp * math.cos(1.307 * phase + 0.4))


def _site_field(k: int, nq: int, h0: float, amp: float) -> float:
    phase = k * 0.619 + nq * 0.037
    return h0 * (1.0 + amp * math.sin(phase)
               + 0.45 * amp * math.cos(2.271 * phase))


def _yy_aniso_factor(a: int, b: int, nq: int, amp: float) -> float:
    lo, hi = (a, b) if a <= b else (b, a)
    phase = lo * 0.513 + hi * 0.887
    return 1.0 + amp * math.sin(phase + 0.9)


# --------------------------------------------------------------------------- #
#  Lattice Pauli-string generators                                            #
# --------------------------------------------------------------------------- #

def _site_index_2d(i: int, j: int, w_p1: int) -> int:
    return j * w_p1 + i


def ising_2d(w: int, h: int,
             phys: Optional[LatticePhysics] = None) -> Tuple[int, List[Tuple[str, float]]]:
    """Transverse-field Ising on a (w+1)x(h+1) 2D grid (open boundaries).

    H = -J sum_<ij> Z_i Z_j - h sum_i X_i  with spatially varying J_ij, h_i.
    Returns: (n_qubits, [(pauli_string, coeff), ...])
    """
    phys = phys or LatticePhysics()
    nq = (w + 1) * (h + 1)
    out: List[Tuple[str, float]] = []
    for j in range(h + 1):
        for i in range(w):
            a = _site_index_2d(i, j, w + 1)
            b = _site_index_2d(i + 1, j, w + 1)
            ps = ["I"] * nq
            ps[a] = "Z"
            ps[b] = "Z"
            out.append(("".join(ps),
                        _edge_coeff(a, b, nq, phys.J0, phys.edge_amp)))
    for j in range(h):
        for i in range(w + 1):
            a = _site_index_2d(i, j, w + 1)
            b = _site_index_2d(i, j + 1, w + 1)
            ps = ["I"] * nq
            ps[a] = "Z"
            ps[b] = "Z"
            out.append(("".join(ps),
                        _edge_coeff(a, b, nq, phys.J0, phys.edge_amp)))
    for k in range(nq):
        ps = ["I"] * nq
        ps[k] = "X"
        out.append(("".join(ps),
                    _site_field(k, nq, phys.h0, phys.field_amp)))
    return nq, out


def heisenberg_2d(w: int, h: int,
                  phys: Optional[LatticePhysics] = None) -> Tuple[int, List[Tuple[str, float]]]:
    """Anisotropic Heisenberg (XXZ) on a (w+1)x(h+1) 2D grid."""
    phys = phys or LatticePhysics()
    nq = (w + 1) * (h + 1)
    out: List[Tuple[str, float]] = []
    edges: List[Tuple[int, int]] = []
    for j in range(h + 1):
        for i in range(w):
            edges.append((_site_index_2d(i, j, w + 1),
                          _site_index_2d(i + 1, j, w + 1)))
    for j in range(h):
        for i in range(w + 1):
            edges.append((_site_index_2d(i, j, w + 1),
                          _site_index_2d(i, j + 1, w + 1)))
    for a, b in edges:
        w_xx = _edge_coeff(a, b, nq, phys.J_xy, phys.edge_amp)
        w_yy = w_xx * _yy_aniso_factor(a, b, nq, phys.aniso_amp)
        w_zz = _edge_coeff(a, b, nq, phys.J_z, phys.edge_amp)
        for op, coeff in (("X", w_xx), ("Y", w_yy), ("Z", w_zz)):
            ps = ["I"] * nq
            ps[a] = op
            ps[b] = op
            out.append(("".join(ps), coeff))
    return nq, out


def _site_index_3d(i: int, j: int, k: int, w1: int, h1: int) -> int:
    return k * w1 * h1 + j * w1 + i


def _edges_3d(w: int, h: int, l: int) -> List[Tuple[int, int]]:
    """Edges of a (w+1)x(h+1)x(l+1) 3D grid, open boundaries."""
    w1, h1, l1 = w + 1, h + 1, l + 1
    edges: List[Tuple[int, int]] = []
    # x-direction
    for k in range(l1):
        for j in range(h1):
            for i in range(w):
                edges.append((_site_index_3d(i, j, k, w1, h1),
                              _site_index_3d(i + 1, j, k, w1, h1)))
    # y-direction
    for k in range(l1):
        for j in range(h):
            for i in range(w1):
                edges.append((_site_index_3d(i, j, k, w1, h1),
                              _site_index_3d(i, j + 1, k, w1, h1)))
    # z-direction
    for k in range(l):
        for j in range(h1):
            for i in range(w1):
                edges.append((_site_index_3d(i, j, k, w1, h1),
                              _site_index_3d(i, j, k + 1, w1, h1)))
    return edges


def ising_3d(w: int, h: int, l: int,
             phys: Optional[LatticePhysics] = None) -> Tuple[int, List[Tuple[str, float]]]:
    phys = phys or LatticePhysics()
    nq = (w + 1) * (h + 1) * (l + 1)
    out: List[Tuple[str, float]] = []
    for a, b in _edges_3d(w, h, l):
        ps = ["I"] * nq
        ps[a] = "Z"
        ps[b] = "Z"
        out.append(("".join(ps),
                    _edge_coeff(a, b, nq, phys.J0, phys.edge_amp)))
    for k in range(nq):
        ps = ["I"] * nq
        ps[k] = "X"
        out.append(("".join(ps),
                    _site_field(k, nq, phys.h0, phys.field_amp)))
    return nq, out


def heisenberg_3d(w: int, h: int, l: int,
                  phys: Optional[LatticePhysics] = None) -> Tuple[int, List[Tuple[str, float]]]:
    phys = phys or LatticePhysics()
    nq = (w + 1) * (h + 1) * (l + 1)
    out: List[Tuple[str, float]] = []
    for a, b in _edges_3d(w, h, l):
        w_xx = _edge_coeff(a, b, nq, phys.J_xy, phys.edge_amp)
        w_yy = w_xx * _yy_aniso_factor(a, b, nq, phys.aniso_amp)
        w_zz = _edge_coeff(a, b, nq, phys.J_z, phys.edge_amp)
        for op, coeff in (("X", w_xx), ("Y", w_yy), ("Z", w_zz)):
            ps = ["I"] * nq
            ps[a] = op
            ps[b] = op
            out.append(("".join(ps), coeff))
    return nq, out


# --------------------------------------------------------------------------- #
#  Trotter-step QASM emission                                                 #
# --------------------------------------------------------------------------- #

def _pauli_string_index_to_qasm_qubit(string_index: int, n_qubits: int, qubit_offset: int = 0) -> int:
    """Map MSB-first Pauli string index to OpenQASM physical qubit (q[0]=LSB).

    Matches ``pca_compress._embed_1q`` / ``analyze_ncf_full_both._apply_pauli_left``:
    leftmost character is qubit ``n-1``, rightmost is qubit ``0``.
    """
    return (n_qubits - 1 - string_index) + qubit_offset


def _emit_pauli_rotation(
    pauli: str,
    angle: float,
    qubit_offset: int = 0,
) -> List[str]:
    """Emit OpenQASM 2.0 fragment for exp(-i * angle/2 * P).

    Convention: the rz(theta) gate in Qiskit/qelib1 implements
    exp(-i theta/2 Z), so for a rotation of the form exp(-i w P) we
    must call rz(2 w).  We follow the common Trotter convention by
    using the angle exactly as supplied; callers should set
    `angle = 2 * coeff * dt` for time-evolution by `dt`.

    Pauli string indices follow the MSB-first / ``read-pauli`` plist
    convention; gates target ``q[phys]`` with ``q[0]`` = LSB.
    """
    n = len(pauli)
    nontrivial = [(i, p) for i, p in enumerate(pauli) if p != "I"]
    if not nontrivial:
        return []  # global phase, ignore

    pre: List[str] = []
    basis_pre: List[str] = []
    # Basis change (X -> Z via H, Y -> Z via Sdg+H).  Undo must reverse this
    # layer gate-by-gate (H->H, Sdg->S); see pca_compress.emit_pauli_rotation_qasm.
    for si, p in nontrivial:
        gq = _pauli_string_index_to_qasm_qubit(si, n, qubit_offset)
        if p == "X":
            basis_pre.append(f"h q[{gq}];")
        elif p == "Y":
            basis_pre.extend([f"sdg q[{gq}];", f"h q[{gq}];"])
    pre.extend(basis_pre)

    # CNOT parity-tree onto the last (largest string-index) non-trivial qubit
    nt_qubits = [
        _pauli_string_index_to_qasm_qubit(si, n, qubit_offset) for si, _ in nontrivial
    ]
    target = nt_qubits[-1]
    for q in nt_qubits[:-1]:
        pre.append(f"cx q[{q}], q[{target}];")

    # Rotation
    pre.append(f"rz({_format_rz_angle(angle)}) q[{target}];")

    # Undo CNOT tree (in reverse) then basis change (adjoint, reverse order)
    for q in reversed(nt_qubits[:-1]):
        pre.append(f"cx q[{q}], q[{target}];")
    for line in reversed(basis_pre):
        if line.startswith("h "):
            pre.append(line)
        elif line.startswith("sdg "):
            pre.append("s" + line[3:])
    return pre


def _format_rz_angle(angle: float) -> str:
    """Format a rotation angle for OpenQASM (full float precision)."""
    if abs(angle) < 1e-15:
        return "0"
    return f"{angle:.17g}"


def emit_trotter_qasm(
    n_qubits: int,
    paulis_with_coeffs: Sequence[Tuple[str, float]],
    dt: float = 1.0,
    title: str = "",
    comments: bool = False,
) -> str:
    """Build a one-Trotter-step OpenQASM 2.0 string.

    Convention: rz angle is set to ``2 * coeff * dt`` so that the gate
    realises ``exp(-i * coeff * dt * P)`` -- this matches the convention
    used in Paulihedral / NCF / Rustiq.
    """
    # NOTE: qsyn's OpenQASM reader does not accept ``//`` comment lines, so
    # comments are disabled by default to keep the output directly readable
    # by the CPF pipeline.  Pass ``comments=True`` for human-facing dumps.
    lines: List[str] = []
    lines.append("OPENQASM 2.0;")
    lines.append('include "qelib1.inc";')
    if comments:
        if title:
            lines.append(f"// {title}")
        lines.append(f"// One Trotter step (dt = {dt}); "
                     f"{len(paulis_with_coeffs)} Pauli rotations")
    lines.append(f"qreg q[{n_qubits}];")
    lines.append("")
    for ps, w in paulis_with_coeffs:
        coeff = _hamiltonian_coeff(w)
        angle = 2.0 * coeff * dt
        if abs(angle) < 1e-15:
            continue
        lines.extend(_emit_pauli_rotation(ps, angle))
    lines.append("")
    return "\n".join(lines)


# --------------------------------------------------------------------------- #
#  Molecule Hamiltonian loaders (PySCF / Hamlib)                              #
# --------------------------------------------------------------------------- #

def _hamiltonian_coeff(w: object) -> float:
    """Coerce a Pauli coefficient to the real Hamiltonian term weight w_j."""
    if hasattr(w, "real") and not isinstance(w, (int, float, complex)):
        return float(getattr(w, "real"))
    if isinstance(w, complex):
        if abs(w.imag) > 1e-12 and abs(w.real) <= 1e-12:
            return float(w.imag)
        return float(w.real)
    return float(w)


def load_molecule_pickle(name: str) -> Tuple[int, List[Tuple[str, float]]]:
    """Load a Paulihedral PySCF molecular Hamiltonian pickle.

    Returns (n_qubits, [(pauli_string, w_j), ...]) with real PySCF
    integrals as w_j (same convention as NCF / Rustiq Trotter step).
    """
    path = os.path.join(PH_DATA_DIR, f"{name}.pickle")
    with open(path, "rb") as f:
        data = pickle.load(f)
    flat: List = []
    for entry in data:
        if isinstance(entry, list):
            flat.extend(entry)
        else:
            flat.append(entry)
    n_qubits = len(flat[0].ps)
    out: List[Tuple[str, float]] = []
    for ps in flat:
        out.append((ps.ps, _hamiltonian_coeff(ps.coeff)))
    return n_qubits, out


def load_molecule_json(name: str) -> Tuple[int, List[Tuple[str, float]]]:
    """Load a Hamlib / PySCF JSON Hamiltonian (``paulis`` + ``coeffs``)."""
    path = os.path.join(HAMLIB_DIR, f"{name}.json")
    with open(path, "r") as f:
        d = json.load(f)
    n_qubits = int(d["num_qubits"])
    out = [(p, float(c)) for p, c in zip(d["paulis"], d["coeffs"])]
    return n_qubits, out


# --------------------------------------------------------------------------- #
#  Driver                                                                     #
# --------------------------------------------------------------------------- #

# NCF Table IV mapping for the lattice benchmarks.
LATTICE_TABLE = [
    ("Ising-2D-30",      "ising_2d",      (4, 5),          30,  79),
    ("Ising-2D-60",      "ising_2d",      (5, 9),          60, 164),
    ("Ising-3D-30",      "ising_3d",      (1, 4, 2),       30,  89),
    ("Ising-3D-60",      "ising_3d",      (2, 3, 4),       60, 193),
    ("Heisenberg-2D-30", "heisenberg_2d", (4, 5),          30, 147),
    ("Heisenberg-2D-60", "heisenberg_2d", (5, 9),          60, 312),
    ("Heisenberg-3D-30", "heisenberg_3d", (1, 4, 2),       30, 177),
    ("Heisenberg-3D-60", "heisenberg_3d", (2, 3, 4),       60, 399),
]

# NCF Table IV mapping for the molecule benchmarks.  ``ham_source`` is
# either the basename of a paulihedral pickle (without ``.pickle``) or
# the basename of a Phoenix Hamlib QASM (``hamlib:<basename>``).
MOLECULE_TABLE = [
    ("LiH",  12,   630, "json:HF-BK12"),
    ("H2O",  14,  1085, "pickle:H2O"),
    ("N2",   20,  2950, "pickle:N2"),
    ("H2S",  22,  6245, "pickle:H2S"),
    ("CO2",  30, 16121, "pickle:CO2"),
]


def _gen_lattice_one(label: str, fn_name: str, args: Tuple[int, ...],
                     phys: LatticePhysics) -> Tuple[str, int, int]:
    fn = globals()[fn_name]
    nq, plist = fn(*args, phys=phys)
    title = (f"NCF benchmark: {label} ({fn_name}{args}); "
             f"dt={phys.dt}, varying J/h")
    return emit_trotter_qasm(nq, plist, dt=phys.dt, title=title), nq, len(plist)


def _gen_molecule_one(label: str, ham_source: str, dt: float) -> Tuple[str, int, int]:
    if ham_source.startswith("pickle:"):
        name = ham_source.split(":", 1)[1]
        nq, plist = load_molecule_pickle(name)
        src_note = f"paulihedral pickle: {name} (PySCF)"
    elif ham_source.startswith("json:"):
        name = ham_source.split(":", 1)[1]
        nq, plist = load_molecule_json(name)
        src_note = f"Hamlib JSON: {name} (PySCF)"
    else:
        raise ValueError(f"Unknown ham_source: {ham_source} "
                         f"(use pickle:<name> or json:<name>)")

    qasm = emit_trotter_qasm(
        nq, plist, dt=dt,
        title=f"NCF benchmark: {label} ({src_note})",
    )
    return qasm, nq, len(plist)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--all", action="store_true",
                        help="Generate every NCF benchmark (default if no flag).")
    parser.add_argument("--lattice", action="store_true",
                        help="Generate the 8 lattice benchmarks.")
    parser.add_argument("--molecule", nargs="*",
                        help="Generate the listed molecule benchmarks "
                             "(default: all 5 if --all is given).")
    parser.add_argument("--dt", type=float, default=0.05,
                        help="Trotter time step (default: 0.05 for lattice; "
                             "also used for molecule re-emission).")
    parser.add_argument("--uniform-lattice", action="store_true",
                        help="Legacy toy model: all lattice coeffs = 1.0, "
                             "dt=1.0 (uniform rz(2.0)).")
    parser.add_argument("--verify", action="store_true",
                        help="Print the Pauli-string count vs. NCF Table IV "
                             "and exit without writing files.")
    args = parser.parse_args()

    do_lattice = args.lattice or args.all or (
        not args.lattice and args.molecule is None)
    do_molecule_all = args.all or (not args.lattice and args.molecule is None)
    selected_molecules = (
        args.molecule if args.molecule else
        ([m[0] for m in MOLECULE_TABLE] if do_molecule_all else [])
    )

    if args.uniform_lattice:
        lattice_phys = make_lattice_physics(dt=1.0, J0=1.0, h0=1.0,
                                          J_xy=1.0, J_z=1.0,
                                          edge_amp=0.0, field_amp=0.0,
                                          aniso_amp=0.0)
    else:
        lattice_phys = make_lattice_physics(dt=args.dt)

    if args.verify:
        print(f"{'benchmark':<22} {'NCF #P':>8}  {'computed':>9}  match")
        for label, fn_name, fn_args, nq_exp, n_exp in LATTICE_TABLE:
            _, nq, nP = _gen_lattice_one(label, fn_name, fn_args, lattice_phys)
            ok = "OK" if (nq == nq_exp and nP == n_exp) else "MISMATCH"
            print(f"{label:<22} {n_exp:>8}  {nP:>9}  {ok}")
        for label, nq_exp, n_exp, src in MOLECULE_TABLE:
            try:
                _, nq, nP = _gen_molecule_one(label, src, args.dt)
            except FileNotFoundError as e:
                print(f"{label:<22} {n_exp:>8}  {'-':>9}  MISSING ({e})")
                continue
            ok = "OK" if nq == nq_exp else f"qubits={nq} (NCF {nq_exp})"
            print(f"{label:<22} {n_exp:>8}  {nP:>9}  {ok}")
        return

    os.makedirs(LATTICE_OUT, exist_ok=True)
    os.makedirs(MOLECULE_OUT, exist_ok=True)

    if do_lattice:
        for label, fn_name, fn_args, nq_exp, n_exp in LATTICE_TABLE:
            qasm, nq, nP = _gen_lattice_one(label, fn_name, fn_args, lattice_phys)
            out_path = os.path.join(LATTICE_OUT, f"{label}.qasm")
            with open(out_path, "w") as f:
                f.write(qasm)
            mark = "OK" if (nq == nq_exp and nP == n_exp) else "MISMATCH"
            print(f"[lattice] {label:<22} qubits={nq} paulis={nP} ({mark}) -> "
                  f"{os.path.relpath(out_path, HERE)}")

    for label in selected_molecules:
        for ml_label, nq_exp, n_exp, src in MOLECULE_TABLE:
            if ml_label == label:
                break
        else:
            print(f"[molecule] unknown: {label}", file=sys.stderr)
            continue
        try:
            qasm, nq, nP = _gen_molecule_one(label, src, args.dt)
        except FileNotFoundError as e:
            print(f"[molecule] {label}: source not found ({e})", file=sys.stderr)
            continue
        out_path = os.path.join(MOLECULE_OUT, f"{label}.qasm")
        with open(out_path, "w") as f:
            f.write(qasm)
        mark = "OK" if nq == nq_exp else f"qubits={nq} (NCF {nq_exp})"
        print(f"[molecule] {label:<6} qubits={nq} paulis={nP} ({mark}) -> "
              f"{os.path.relpath(out_path, HERE)}")


if __name__ == "__main__":
    main()
