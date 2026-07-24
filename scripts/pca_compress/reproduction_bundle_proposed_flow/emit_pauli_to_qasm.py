#!/usr/bin/env python3
"""Standalone Pauli-list → OpenQASM 2.0 emitter for this reproduction bundle.

Matches the convention used when packaging ``01_`` / ``02_`` circuits:
``compress_ncf._emit_pauli_circuit_qasm`` → ``gen_benchmark_qasm.emit_trotter_qasm``.

Usage:
  python3 emit_pauli_to_qasm.py path/to/Bench.pauli
  python3 emit_pauli_to_qasm.py path/to/Bench.pauli -o out.qasm
  python3 emit_pauli_to_qasm.py path/to/dir/          # all *.pauli under dir
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import List, Sequence, Tuple

_HEADER_QUBITS_RE = re.compile(r"\((\d+)\s*qubits?\)", re.IGNORECASE)


def parse_pauli_file(path: Path) -> Tuple[int, List[Tuple[str, float]], str]:
    """Return (n_qubits, [(pauli, theta), ...], title)."""
    text = path.read_text(encoding="utf-8")
    rotations: List[Tuple[str, float]] = []
    n_qubits: int | None = None
    title = path.stem

    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("#"):
            m = _HEADER_QUBITS_RE.search(line)
            if m:
                n_qubits = int(m.group(1))
            # e.g. "# from-pauli-list input: LiH (12 qubits)"
            if ":" in line:
                title = line.split(":", 1)[1].strip()
            continue
        parts = line.split()
        if len(parts) < 2:
            raise ValueError(f"{path}: bad line: {raw!r}")
        pauli, theta_s = parts[0], parts[1]
        if not re.fullmatch(r"[IXYZixyz]+", pauli):
            raise ValueError(f"{path}: invalid Pauli string: {pauli!r}")
        pauli = pauli.upper()
        rotations.append((pauli, float(theta_s)))
        if n_qubits is None:
            n_qubits = len(pauli)
        elif len(pauli) != n_qubits:
            raise ValueError(
                f"{path}: Pauli length {len(pauli)} != n_qubits {n_qubits}"
            )

    if n_qubits is None:
        raise ValueError(f"{path}: empty Pauli list")
    return n_qubits, rotations, title


def _pauli_string_index_to_qasm_qubit(
    string_index: int, n_qubits: int, qubit_offset: int = 0
) -> int:
    """MSB-first Pauli string index → OpenQASM qubit (q[0] = LSB)."""
    return (n_qubits - 1 - string_index) + qubit_offset


def _format_rz_angle(angle: float) -> str:
    if abs(angle) < 1e-15:
        return "0"
    return f"{angle:.17g}"


def _emit_pauli_rotation(pauli: str, angle: float) -> List[str]:
    """OpenQASM fragment for one Pauli rotation (angle = rz argument)."""
    n = len(pauli)
    nontrivial = [(i, p) for i, p in enumerate(pauli) if p != "I"]
    if not nontrivial:
        return []

    basis_pre: List[str] = []
    for si, p in nontrivial:
        gq = _pauli_string_index_to_qasm_qubit(si, n)
        if p == "X":
            basis_pre.append(f"h q[{gq}];")
        elif p == "Y":
            basis_pre.extend([f"sdg q[{gq}];", f"h q[{gq}];"])

    lines: List[str] = list(basis_pre)
    nt_qubits = [_pauli_string_index_to_qasm_qubit(si, n) for si, _ in nontrivial]
    target = nt_qubits[-1]
    for q in nt_qubits[:-1]:
        lines.append(f"cx q[{q}], q[{target}];")
    lines.append(f"rz({_format_rz_angle(angle)}) q[{target}];")
    for q in reversed(nt_qubits[:-1]):
        lines.append(f"cx q[{q}], q[{target}];")
    for line in reversed(basis_pre):
        if line.startswith("h "):
            lines.append(line)
        elif line.startswith("sdg "):
            lines.append("s" + line[3:])
    return lines


def emit_qasm(
    n_qubits: int,
    rotations: Sequence[Tuple[str, float]],
    *,
    title: str = "",
    comments: bool = False,
) -> str:
    """Emit OpenQASM 2.0 from Pauli rotations with angles θ (list convention).

    Same as packaging: pass coeff = θ/2 into the Trotter emitter so that
    ``rz`` receives ``2 * coeff = θ``.
    """
    lines: List[str] = [
        "OPENQASM 2.0;",
        'include "qelib1.inc";',
    ]
    if comments:
        if title:
            lines.append(f"// {title}")
        lines.append(f"// {len(rotations)} Pauli rotations")
    lines.append(f"qreg q[{n_qubits}];")
    lines.append("")

    for pauli, theta in rotations:
        # Match compress_ncf._emit_pauli_circuit_qasm:
        #   plist = (pauli, theta/2); emit uses angle = 2 * coeff * dt
        angle = float(theta)
        if abs(angle) < 1e-15:
            continue
        lines.extend(_emit_pauli_rotation(pauli, angle))
    lines.append("")
    return "\n".join(lines)


def pauli_path_to_qasm(path: Path, out: Path | None = None) -> Path:
    n_qubits, rotations, title = parse_pauli_file(path)
    text = emit_qasm(n_qubits, rotations, title=title)
    dest = out if out is not None else path.with_suffix(".qasm")
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(text, encoding="utf-8")
    return dest


def main(argv: List[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description="Emit OpenQASM 2.0 from a .pauli list (bundle standalone)."
    )
    ap.add_argument(
        "input",
        type=Path,
        help="Path to a .pauli file, or a directory to process recursively",
    )
    ap.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="Output .qasm path (only when input is a single file)",
    )
    ap.add_argument(
        "--check",
        action="store_true",
        help="Compare emitted QASM to sibling .qasm (or -o) and exit non-zero on mismatch",
    )
    args = ap.parse_args(argv)

    inp: Path = args.input
    if not inp.exists():
        print(f"error: not found: {inp}", file=sys.stderr)
        return 1

    if inp.is_dir():
        if args.output is not None:
            print("error: -o cannot be used with a directory", file=sys.stderr)
            return 1
        files = sorted(inp.rglob("*.pauli"))
        if not files:
            print(f"error: no .pauli under {inp}", file=sys.stderr)
            return 1
        bad = 0
        for p in files:
            n_qubits, rotations, title = parse_pauli_file(p)
            text = emit_qasm(n_qubits, rotations, title=title)
            if args.check:
                ref = p.with_suffix(".qasm")
                if not ref.exists():
                    print(f"missing reference {ref}", file=sys.stderr)
                    bad += 1
                elif ref.read_text(encoding="utf-8") != text:
                    print(f"MISMATCH vs {ref}", file=sys.stderr)
                    bad += 1
                else:
                    print(f"OK {ref.relative_to(inp)}  ({len(rotations)} rot)")
            else:
                dest = p.with_suffix(".qasm")
                dest.write_text(text, encoding="utf-8")
                print(f"wrote {dest}  ({len(rotations)} rotations, {n_qubits} qubits)")
        return 1 if bad else 0

    if inp.suffix != ".pauli":
        print("error: input file must end with .pauli", file=sys.stderr)
        return 1

    n_qubits, rotations, title = parse_pauli_file(inp)
    text = emit_qasm(n_qubits, rotations, title=title)

    if args.check:
        ref = args.output if args.output is not None else inp.with_suffix(".qasm")
        if not ref.exists():
            print(f"error: reference missing: {ref}", file=sys.stderr)
            return 1
        if ref.read_text(encoding="utf-8") != text:
            print(f"MISMATCH: emitted != {ref}", file=sys.stderr)
            return 1
        print(f"OK: matches {ref} ({len(rotations)} rotations, {n_qubits} qubits)")
        return 0

    dest = pauli_path_to_qasm(inp, args.output)
    print(f"wrote {dest}  ({len(rotations)} rotations, {n_qubits} qubits)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
