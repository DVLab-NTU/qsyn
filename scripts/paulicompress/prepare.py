"""Build ``01_original_benchmarks/`` from Paulihedral / lattice sources."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import List, Optional, Sequence, Tuple

from util import BENCHMARK_DIR, write_pauli_list

# Import gen_benchmark_qasm without installing as a package.
sys.path.insert(0, str(BENCHMARK_DIR))
import gen_benchmark_qasm as gen  # noqa: E402


def _rotations_from_plist(
    plist: Sequence[Tuple[str, float]],
    dt: float,
) -> List[Tuple[str, float]]:
    """Return (Pauli, rz-angle) with ``angle = 2 * coeff * dt`` (Trotter convention)."""
    out: List[Tuple[str, float]] = []
    for ps, w in plist:
        coeff = gen._hamiltonian_coeff(w)
        angle = 2.0 * coeff * dt
        if abs(angle) < 1e-15:
            continue
        out.append((ps, angle))
    return out


def _write_pair(
    out_root: Path,
    label: str,
    n_qubits: int,
    plist: Sequence[Tuple[str, float]],
    dt: float,
    title: str,
) -> Path:
    rotations = _rotations_from_plist(plist, dt)
    qasm = gen.emit_trotter_qasm(n_qubits, plist, dt=dt, title=title)
    bench_dir = out_root / label
    bench_dir.mkdir(parents=True, exist_ok=True)
    pauli_path = bench_dir / f"{label}.pauli"
    qasm_path = bench_dir / f"{label}.qasm"
    write_pauli_list(pauli_path, label, n_qubits, rotations)
    qasm_path.write_text(qasm, encoding="utf-8")
    print(f"[prepare] {label}: {n_qubits}q, {len(rotations)} rotations → {bench_dir}")
    return bench_dir


def prepare_benchmarks(
    out_root: Path,
    *,
    names: Optional[Sequence[str]] = None,
    dt: float = 0.05,
) -> List[Path]:
    """Generate ``<out>/<Bench>/{Bench}.pauli,.qasm}`` for selected NCF benchmarks."""
    out_root = Path(out_root)
    out_root.mkdir(parents=True, exist_ok=True)
    want = set(names) if names else None
    written: List[Path] = []

    phys = gen.make_lattice_physics(dt=dt)
    for label, fn_name, fn_args, _nq_exp, _n_exp in gen.LATTICE_TABLE:
        if want is not None and label not in want:
            continue
        fn = getattr(gen, fn_name)
        nq, plist = fn(*fn_args, phys=phys)
        title = f"NCF benchmark: {label} ({fn_name}{fn_args}); dt={dt}"
        written.append(_write_pair(out_root, label, nq, plist, dt, title))

    for label, _nq_exp, _n_exp, src in gen.MOLECULE_TABLE:
        if want is not None and label not in want:
            continue
        if src.startswith("pickle:"):
            name = src.split(":", 1)[1]
            nq, plist = gen.load_molecule_pickle(name)
            note = f"paulihedral pickle: {name}"
        elif src.startswith("json:"):
            name = src.split(":", 1)[1]
            nq, plist = gen.load_molecule_json(name)
            note = f"Hamlib JSON: {name}"
        else:
            raise ValueError(src)
        title = f"NCF benchmark: {label} ({note}); dt={dt}"
        written.append(_write_pair(out_root, label, nq, plist, dt, title))

    if want is not None:
        missing = want - {p.name for p in written}
        if missing:
            raise SystemExit(f"unknown benchmark(s): {sorted(missing)}")
    return written


def main(argv: Optional[Sequence[str]] = None) -> None:
    p = argparse.ArgumentParser(
        description="Paulihedral/lattice → 01_original_benchmarks layout (.pauli + .qasm)",
    )
    p.add_argument(
        "--out",
        type=Path,
        default=Path("out/01_original_benchmarks"),
        help="Output directory (default: out/01_original_benchmarks)",
    )
    p.add_argument(
        "--bench",
        nargs="*",
        default=None,
        help="Subset of benchmarks (default: all 13 NCF Table IV names)",
    )
    p.add_argument("--dt", type=float, default=0.05, help="Trotter step (default 0.05)")
    args = p.parse_args(argv)
    prepare_benchmarks(args.out, names=args.bench, dt=args.dt)
    print(f"[prepare] done → {args.out.resolve()}")


if __name__ == "__main__":
    main()
