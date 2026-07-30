#!/usr/bin/env python3
"""Unitary / process fidelity of PauliCompress outputs vs originals.

For each case and F* tier, compare
  U = ∏ exp(-i θ_k/2 P_k)   (thesis Pauli-list convention)
  V = compressed list
via process fidelity  F_p = |Tr(U† V) / d|²
and the coefficient surrogate F_approx = ∏ |cos(d_P)|.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np

QSYN = Path("/home/chenying/qsyn")
OUT = QSYN / "out" / "paulicompress_10cases"
sys.path.insert(0, str(QSYN / "scripts" / "paulicompress"))

from pauli_list import PauliCircuit, coeff_error, load_pauli_file  # noqa: E402

TIERS = (0.999, 0.99, 0.9)
CASES = [
    "h2_jw_four",
    "LiH_frz_JW",
    "Be2-JW-6",
    "BH-JW-10",
    "uccsd_10",
    "OH-JW10",
    "OH-JW12",
    "H2O_frz_JW_sto3g",
    "Li2-JW14",
    "NH-JW-14",
]

# Dense / exact Trace feasible up to this width; larger → Monte Carlo.
EXACT_MAX_QUBITS = 10
MC_SAMPLES = 128
RNG = np.random.default_rng(0)


def _pauli_apply(psi: np.ndarray, pauli: str) -> np.ndarray:
    """Apply Pauli string (MSB = qubit 0) to statevector."""
    n = len(pauli)
    out = psi.copy()
    dim = psi.shape[0]
    # Apply from qubit 0..n-1; bit (n-1-q) is the little-endian index bit.
    for q, ch in enumerate(pauli):
        if ch == "I":
            continue
        bit = n - 1 - q
        mask = 1 << bit
        if ch == "X":
            # swap amplitudes i <-> i^mask
            idx = np.arange(dim)
            partner = idx ^ mask
            # only update half to avoid double swap
            sel = idx < partner
            a, b = idx[sel], partner[sel]
            out[a], out[b] = out[b].copy(), out[a].copy()
        elif ch == "Z":
            idx = np.arange(dim)
            out[(idx & mask) != 0] *= -1.0
        elif ch == "Y":
            idx = np.arange(dim)
            partner = idx ^ mask
            sel = idx < partner
            a, b = idx[sel], partner[sel]
            # Y: |0><1|*(-i) + |1><0|*(i) on that qubit
            oa, ob = out[a].copy(), out[b].copy()
            out[a] = -1j * ob
            out[b] = 1j * oa
        else:
            raise ValueError(ch)
    return out


def _apply_rot(psi: np.ndarray, pauli: str, theta: float) -> np.ndarray:
    """Apply exp(-i θ/2 P) using cos/sin identity."""
    phi = 0.5 * float(theta)
    c = math.cos(phi)
    s = math.sin(phi)
    ppsi = _pauli_apply(psi, pauli)
    return c * psi - 1j * s * ppsi


def apply_circuit(psi: np.ndarray, pc: PauliCircuit) -> np.ndarray:
    out = psi
    for r in pc.rotations:
        out = _apply_rot(out, r.pauli, r.theta)
    return out


def process_fidelity_exact(u: PauliCircuit, v: PauliCircuit) -> float:
    """F_p = |Tr(U†V)/d|² via computational-basis diagonal sum."""
    n = u.n_qubits
    d = 1 << n
    acc = 0.0 + 0.0j
    for i in range(d):
        e = np.zeros(d, dtype=np.complex128)
        e[i] = 1.0
        # ⟨i|U† V|i⟩ = ⟨U i | V i⟩
        ui = apply_circuit(e, u)
        vi = apply_circuit(e, v)
        acc += np.vdot(ui, vi)
    return float(abs(acc / d) ** 2)


def process_fidelity_mc(u: PauliCircuit, v: PauliCircuit, samples: int) -> tuple[float, float]:
    """Monte-Carlo estimate of Tr(U†V)/d via Haar states; return (F_p, stderr proxy)."""
    n = u.n_qubits
    d = 1 << n
    overlaps = []
    for _ in range(samples):
        z = RNG.normal(size=d) + 1j * RNG.normal(size=d)
        z /= np.linalg.norm(z)
        uz = apply_circuit(z, u)
        vz = apply_circuit(z, v)
        overlaps.append(np.vdot(uz, vz))
    overlaps = np.asarray(overlaps, dtype=np.complex128)
    mean = overlaps.mean()
    # stderr of mean overlap (complex) — rough magnitude stderr
    stderr = float(np.std(np.abs(overlaps)) / math.sqrt(samples))
    return float(abs(mean) ** 2), stderr


def main() -> None:
    rows = []
    print(
        f"{'#':>2} {'Case':22s} {'n':>3} {'F*':>6} {'#rot':>9} "
        f"{'F_approx':>10} {'F_unitary':>10} {'method':>8} {'|Δ|':>10}"
    )
    print("-" * 100)

    for ci, case in enumerate(CASES, 1):
        case_dir = OUT / case
        orig = load_pauli_file(case_dir / f"{case}.pauli", name=case)
        for F in TIERS:
            tag = str(F).replace(".", "p")
            comp_path = case_dir / f"after_F{tag}.pauli"
            comp = load_pauli_file(comp_path, name=f"{case}@{F}")
            _, _, f_approx = coeff_error(orig.rotations, comp.rotations)

            if orig.n_qubits <= EXACT_MAX_QUBITS:
                f_u = process_fidelity_exact(orig, comp)
                method = "exact"
                err = 0.0
            else:
                f_u, err = process_fidelity_mc(orig, comp, MC_SAMPLES)
                method = f"MC{MC_SAMPLES}"

            delta = abs(f_u - f_approx)
            n_after = len(comp.rotations)
            print(
                f"{ci:2d} {case:22s} {orig.n_qubits:3d} {F:6g} "
                f"{len(orig.rotations):4d}->{n_after:<4d} "
                f"{f_approx:10.6f} {f_u:10.6f} {method:>8} {delta:10.2e}"
                + (f"  ±{err:.1e}" if method.startswith("MC") else ""),
                flush=True,
            )
            rows.append(
                {
                    "case": case,
                    "n_qubits": orig.n_qubits,
                    "f_target": F,
                    "n_rot_before": len(orig.rotations),
                    "n_rot_after": n_after,
                    "F_approx": f_approx,
                    "F_unitary": f_u,
                    "method": method,
                    "mc_stderr_abs_overlap": err,
                    "abs_delta_approx_vs_unitary": delta,
                }
            )

    out_json = OUT / "unitary_fidelity.json"
    out_json.write_text(json.dumps({"rows": rows}, indent=2), encoding="utf-8")
    print(f"\nwrote {out_json}")
    print(
        "Convention: U=∏ exp(-i θ/2 P) (PauliCompress list). "
        "F_unitary = |Tr(U†V)/d|². "
        f"Exact for n≤{EXACT_MAX_QUBITS}; else Haar MC ({MC_SAMPLES} samples)."
    )


if __name__ == "__main__":
    main()
