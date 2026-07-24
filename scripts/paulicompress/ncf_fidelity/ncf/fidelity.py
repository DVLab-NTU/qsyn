"""Fidelity of the synthesised NCF circuits.

* ``F_approx`` — product of per-group **aligned** process fidelities
  ``∏_k F_k`` with ``F_k = max_{perm,inv} |Tr(U† V)|/d`` subject to Synthetiq
  distance ≤ ε (matches what Synthetiq actually guarantees).
* ``F_direct`` — product without qubit-permutation alignment (legacy; can be
  **much lower** for ncf2 when Synthetiq matches a permuted target).
* ``F_worst`` — product of per-group **minimum** aligned F among all Synthetiq
  candidate circuits (pessimistic synthesis choice at fixed ε).
* LiH exact statevector cross-check reuses ``lih_unitary_frobenius_verify``.
"""

from __future__ import annotations

import csv
import os
import sys
from typing import Dict, List, Optional

from . import config
from .compile_ncf import CompiledMethod, emit_qasm


def approx_fidelity(cm: CompiledMethod) -> float:
    return cm.product_fidelity


def direct_fidelity(cm: CompiledMethod) -> float:
    return cm.product_fidelity_direct


def worst_fidelity(cm: CompiledMethod) -> float:
    return cm.product_fidelity_worst


def lih_exact_fidelity(cm: CompiledMethod, work_qasm: str) -> Optional[float]:
    """Optional LiH statevector check (needs ``pca_compress/lih_unitary_frobenius_verify``)."""
    if cm.benchmark != "LiH":
        return None
    pca = str(config.PCA_COMPRESS_DIR)
    if pca not in sys.path:
        sys.path.insert(0, pca)
    try:
        import lih_unitary_frobenius_verify as lih  # noqa: E402
    except ImportError as exc:
        raise ImportError(
            "LiH exact fidelity needs scripts/pca_compress/lih_unitary_frobenius_verify.py"
        ) from exc

    os.makedirs(os.path.dirname(work_qasm) or ".", exist_ok=True)
    with open(work_qasm, "w", encoding="utf-8") as f:
        f.write(emit_qasm(cm))
    raw = lih._pauli_arrays(config.resolve_pauli_path("LiH"))
    return float(lih.process_fidelity_pauli_vs_qasm(raw, work_qasm))


FIDELITY_FIELDS = [
    "benchmark", "method", "n_qubits", "n_paulis", "n_unitaries", "eps_eff",
    "t_count", "F_approx", "F_direct", "F_worst", "infidelity_approx",
    "F_exact_LiH", "delta_exact_vs_approx", "note",
]


def fidelity_row(cm: CompiledMethod, f_exact: Optional[float] = None,
                 note: str = "") -> Dict[str, object]:
    fa = approx_fidelity(cm)
    fd = direct_fidelity(cm)
    fw = worst_fidelity(cm)
    row = {
        "benchmark": cm.benchmark,
        "method": cm.method,
        "n_qubits": cm.n_qubits,
        "n_paulis": cm.n_paulis,
        "n_unitaries": cm.n_unitaries,
        "eps_eff": f"{cm.eps:.6g}",
        "t_count": cm.t_count,
        "F_approx": f"{fa:.8f}",
        "F_direct": f"{fd:.8f}",
        "F_worst": f"{fw:.8f}",
        "infidelity_approx": f"{1 - fa:.3e}",
        "F_exact_LiH": f"{f_exact:.8f}" if f_exact is not None else "",
        "delta_exact_vs_approx": f"{abs(f_exact - fa):.3e}" if f_exact is not None else "",
        "note": note,
    }
    return row


def write_fidelity_csv(rows: List[Dict[str, object]], path: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIDELITY_FIELDS)
        w.writeheader()
        for r in rows:
            w.writerow(r)


def read_fidelity_csv(path: str) -> List[Dict[str, object]]:
    if not os.path.isfile(path):
        return []
    with open(path, encoding="utf-8") as f:
        return list(csv.DictReader(f))


def merge_fidelity_rows(existing: List[Dict[str, object]],
                        new_rows: List[Dict[str, object]]) -> List[Dict[str, object]]:
    idx = {(r["benchmark"], r["method"]): i for i, r in enumerate(existing)}
    for r in new_rows:
        key = (r["benchmark"], r["method"])
        if key in idx:
            existing[idx[key]] = r
        else:
            existing.append(r)
    return existing
