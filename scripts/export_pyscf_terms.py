#!/usr/bin/env python3
"""Export PySCF / OpenFermion Pauli Hamiltonian terms for qsyn ``tableau from-terms``.

Output formats (read by qsyn ``load_pauli_terms_file``):
  - JSON (recommended): version, molecule, basis, encoding, n_qubits, terms[]
  - .terms text: ``<id> <pauli> coeff=<c> angle=<a>``

Example:
  python3 scripts/export_pyscf_terms.py --preset h2 -o h2_jw.json
  qsyn -c "tableau from-terms h2_jw.json; tableau optimize collapse; tableau optimize ncf"
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


H2_PRESET = {
    "version": 1,
    "molecule": "H2",
    "basis": "sto-3g",
    "encoding": "jordan_wigner",
    "n_qubits": 4,
    "terms": [
        {"id": 0, "pauli": "ZIII", "coeff": 0.13716572937099508},
        {"id": 1, "pauli": "IZII", "coeff": 0.13716572937099503},
        {"id": 2, "pauli": "IIZI", "coeff": -0.13036292057109103},
        {"id": 3, "pauli": "IIIZ", "coeff": -0.13036292057109103},
        {"id": 4, "pauli": "ZZII", "coeff": 0.15660062488237947},
        {"id": 5, "pauli": "YXXY", "coeff": 0.049197645871367546},
        {"id": 6, "pauli": "YYXX", "coeff": -0.049197645871367546},
        {"id": 7, "pauli": "XXYY", "coeff": -0.049197645871367546},
        {"id": 8, "pauli": "XYYX", "coeff": 0.049197645871367546},
        {"id": 9, "pauli": "ZIZI", "coeff": 0.10622904490856078},
        {"id": 10, "pauli": "ZIIZ", "coeff": 0.15542669077992832},
        {"id": 11, "pauli": "IZZI", "coeff": 0.15542669077992832},
        {"id": 12, "pauli": "IZIZ", "coeff": 0.10622904490856078},
        {"id": 13, "pauli": "IIZZ", "coeff": 0.16326768673564343},
    ],
}


def _openfermion_term_to_string(term, n_qubits: int) -> str:
    chars = ["I"] * n_qubits
    for qubit_idx, axis in term:
        chars[qubit_idx] = axis
    return "".join(chars)


def export_openfermion(geometry: list, basis: str, encoding: str) -> dict:
    import openfermion as of
    import openfermionpyscf as ofpyscf

    ham = ofpyscf.generate_molecular_hamiltonian(geometry, basis, 1, 0)
    ham_q = of.jordan_wigner(of.get_fermion_operator(ham))
    if encoding not in ("jw", "jordan_wigner"):
        raise ValueError(f"Unsupported encoding: {encoding}")

    n_qubits = max((q for term in ham_q.terms for q, _ in term), default=-1) + 1
    terms = []
    for term, coeff in sorted(ham_q.terms.items(), key=lambda kv: str(kv[0])):
        pauli_str = _openfermion_term_to_string(term, n_qubits)
        if set(pauli_str) == {"I"}:
            continue
        c = float(coeff.real)
        terms.append({"id": len(terms), "pauli": pauli_str, "coeff": c, "angle": c})
    return {
        "version": 1,
        "molecule": repr(geometry),
        "basis": basis,
        "encoding": "jordan_wigner",
        "n_qubits": n_qubits,
        "terms": terms,
    }


def write_json(data: dict, path: Path) -> None:
    path.write_text(json.dumps(data, indent=2) + "\n")


def write_terms_text(data: dict, path: Path) -> None:
    lines = [f"# n_qubits={data['n_qubits']}"]
    for t in data["terms"]:
        coeff = t.get("coeff", t.get("angle", 0))
        angle = t.get("angle", coeff)
        lines.append(f"{t['id']} {t['pauli']} coeff={coeff} angle={angle}")
    path.write_text("\n".join(lines) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-o", "--output", type=Path, required=True, help="Output .json or .terms path")
    parser.add_argument("--preset", choices=["h2"], help="Built-in molecule preset (no PySCF required)")
    parser.add_argument("--basis", default="sto-3g")
    parser.add_argument("--encoding", default="jw", choices=["jw"])
    parser.add_argument(
        "--pyscf",
        action="store_true",
        help="Use OpenFermion+PySCF (requires openfermion, openfermionpyscf, pyscf)",
    )
    args = parser.parse_args()

    if args.preset == "h2":
        data = dict(H2_PRESET)
    elif args.pyscf:
        data = export_openfermion([("H", (0.0, 0.0, 0.0)), ("H", (0.0, 0.0, 0.74))], args.basis, args.encoding)
    else:
        parser.error("Specify --preset h2 or --pyscf")

    if args.output.suffix == ".json":
        write_json(data, args.output)
    else:
        write_terms_text(data, args.output)
    print(f"Wrote {len(data['terms'])} terms → {args.output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
