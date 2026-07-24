#!/usr/bin/env python3
"""cpf_bench.py -- end-to-end CPF benchmark runner + Markdown report.

For each input QASM file:

  1. Run qsyn's `qcir cpf-optimize` (and its `--no-full` companion) on
     the circuit, writing the result to a temporary QASM file.
  2. Verify equivalence against the original using `verify_equiv.py`
     (i.e. Qiskit `Operator` comparison up to a global phase).
  3. Optionally compare gate counts against a fresh BQSKit recompile
     when `--with-bqskit` is supplied and the package is installed.
  4. Aggregate the results into a Markdown table and (by default)
     write it back to `docs/benchmark_results.md`.

Example:

    python scripts/cpf_bench.py \\
        --qsyn build/qsyn \\
        --inputs testcase/benchmark/qasm \\
        --out docs/benchmark_results.md

Dependencies: qiskit (for verify_equiv); bqskit only when
`--with-bqskit` is requested.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple

# Reuse verify_equiv as a library.
SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))
import verify_equiv  # noqa: E402


@dataclass
class Row:
    name: str
    n_qubits: int
    base_gates: int
    base_cnots: int
    cpf_gates: int
    cpf_cnots: int
    cpf_runtime_s: float
    cpf_equivalent: bool
    cpf_residual: float
    cpf_no_full_gates: int
    cpf_no_full_cnots: int
    cpf_rot_before: Optional[int] = None
    cpf_rot_after: Optional[int] = None
    cpf_rz_gates: Optional[int] = None
    bqskit_gates: Optional[int] = None
    bqskit_cnots: Optional[int] = None
    bqskit_runtime_s: Optional[float] = None

    @staticmethod
    def header() -> str:
        return ("| benchmark | qubits | "
                "input gates / CX | "
                "cpf gates / CX | "
                "cpf --no-full g/CX | "
                "rot before→after | "
                "out RZ | "
                "bqskit g/CX | "
                "equiv | runtime |")

    @staticmethod
    def sep() -> str:
        return "|" + "|".join(["-" * 10] * 9) + "|"

    def to_md(self) -> str:
        equiv = "yes" if self.cpf_equivalent else f"NO ({self.cpf_residual:.1e})"
        cpf_runtime = f"{self.cpf_runtime_s * 1000:.1f}ms"
        bqskit = (
            f"{self.bqskit_gates}/{self.bqskit_cnots}"
            if self.bqskit_gates is not None
            else "-"
        )
        if self.bqskit_runtime_s is not None:
            cpf_runtime += f" / bqskit {self.bqskit_runtime_s:.2f}s"
        rot = "-"
        if self.cpf_rot_before is not None and self.cpf_rot_after is not None:
            rot = f"{self.cpf_rot_before}→{self.cpf_rot_after}"
        rz = str(self.cpf_rz_gates) if self.cpf_rz_gates is not None else "-"
        return (f"| `{self.name}` | {self.n_qubits} | "
                f"{self.base_gates}/{self.base_cnots} | "
                f"{self.cpf_gates}/{self.cpf_cnots} | "
                f"{self.cpf_no_full_gates}/{self.cpf_no_full_cnots} | "
                f"{rot} | {rz} | "
                f"{bqskit} | {equiv} | {cpf_runtime} |")


def _count_gates(qasm: Path):
    from qiskit import QuantumCircuit
    qc = QuantumCircuit.from_qasm_file(str(qasm))
    n_cx = sum(1 for instr in qc.data if instr.operation.name in ("cx", "cnot"))
    n_rz = sum(1 for instr in qc.data if instr.operation.name == "rz")
    return qc.num_qubits, len(qc.data), n_cx, n_rz


_ROT_RE = re.compile(
    r"cpf-(?:optimize|pipeline): rotations (\d+) -> (\d+)",
    re.IGNORECASE,
)


def _run_qsyn(qsyn_bin: Path, commands: List[str]) -> Tuple[str, str]:
    """Drive qsyn with `-c` (semicolon-separated commands). Returns (stdout, stderr)."""
    cmd = [str(qsyn_bin), "-q", "-c", ";".join(commands)]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    if result.returncode != 0:
        raise RuntimeError(
            f"qsyn failed:\n  cmd  = {cmd}\n  out  = {result.stdout}\n  err  = {result.stderr}")
    return result.stdout, result.stderr


def _parse_rotation_stats(log: str) -> Tuple[Optional[int], Optional[int]]:
    m = _ROT_RE.search(log)
    if not m:
        return None, None
    return int(m.group(1)), int(m.group(2))


def _qsyn_cpf_optimize(qsyn_bin: Path, src: Path, dst: Path, no_full: bool) -> Tuple[float, Optional[int], Optional[int]]:
    """Run `qcir read; qcir cpf-optimize [--no-full]; qcir write` in qsyn."""
    flag = " --no-full" if no_full else ""
    cmds = [
        f"qcir read {src}",
        f"qcir cpf-optimize --skip-u3cx -r{flag}",
        f"qcir write {dst}",
        "quit -f",
    ]
    start = time.perf_counter()
    out, err = _run_qsyn(qsyn_bin, cmds)
    rot = _parse_rotation_stats(out + err)
    return time.perf_counter() - start, rot[0], rot[1]


def _maybe_bqskit(src: Path, opt_level: int) -> Optional[tuple]:
    """Return `(gates, cnots, runtime_s)` from a BQSKit recompile,
    or `None` if bqskit is unavailable / the recompile fails."""
    try:
        from bqskit import compile as bqskit_compile
        from bqskit.ir import Circuit
        from bqskit.ir.gates import CNOTGate
    except ImportError:
        return None
    try:
        circuit = Circuit.from_file(str(src))
        start = time.perf_counter()
        out = bqskit_compile(circuit, optimization_level=opt_level)
        runtime = time.perf_counter() - start
        n_cnots = sum(1 for op in out if isinstance(op.gate, CNOTGate))
        return out.num_operations, n_cnots, runtime
    except Exception as exc:  # noqa: BLE001 - best-effort comparison
        print(f"  [warn] bqskit recompile failed: {exc}", file=sys.stderr)
        return None


def benchmark_one(qsyn_bin: Path, src: Path, opt_level: int, with_bqskit: bool) -> Row:
    n_qubits, base_gates, base_cnots, _ = _count_gates(src)

    with tempfile.TemporaryDirectory() as td:
        cpf_path     = Path(td) / "cpf_full.qasm"
        cpf_no_full  = Path(td) / "cpf_lite.qasm"
        rt_full, rot_before, rot_after = _qsyn_cpf_optimize(qsyn_bin, src, cpf_path, no_full=False)
        _      = _qsyn_cpf_optimize(qsyn_bin, src, cpf_no_full, no_full=True)

        u_src   = verify_equiv.load_unitary(src)
        u_cpf   = verify_equiv.load_unitary(cpf_path)
        equiv, residual = verify_equiv.is_equivalent(u_src, u_cpf)
        _, cpf_gates, cpf_cnots, cpf_rz = _count_gates(cpf_path)
        _, lite_gates, lite_cnots, _ = _count_gates(cpf_no_full)

        bqskit_gates = bqskit_cnots = None
        bqskit_runtime: Optional[float] = None
        if with_bqskit:
            res = _maybe_bqskit(src, opt_level)
            if res is not None:
                bqskit_gates, bqskit_cnots, bqskit_runtime = res

    return Row(
        name=src.name,
        n_qubits=n_qubits,
        base_gates=base_gates,
        base_cnots=base_cnots,
        cpf_gates=cpf_gates,
        cpf_cnots=cpf_cnots,
        cpf_runtime_s=rt_full,
        cpf_equivalent=equiv,
        cpf_residual=residual,
        cpf_no_full_gates=lite_gates,
        cpf_no_full_cnots=lite_cnots,
        cpf_rot_before=rot_before,
        cpf_rot_after=rot_after,
        cpf_rz_gates=cpf_rz,
        bqskit_gates=bqskit_gates,
        bqskit_cnots=bqskit_cnots,
        bqskit_runtime_s=bqskit_runtime,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--qsyn", type=Path, default=Path("build/qsyn"),
                        help="path to the qsyn binary (default: build/qsyn)")
    parser.add_argument("--inputs", type=Path, default=Path("testcase/benchmark/qasm"),
                        help="directory containing .qasm benchmark inputs")
    parser.add_argument("--out", type=Path, default=Path("docs/benchmark_results.md"),
                        help="destination of the Markdown report")
    parser.add_argument("--with-bqskit", action="store_true",
                        help="also recompile each benchmark with BQSKit for comparison")
    parser.add_argument("--bqskit-opt-level", type=int, default=1)
    args = parser.parse_args()

    if not args.qsyn.exists():
        raise SystemExit(f"qsyn binary {args.qsyn} not found; run `make` first or pass --qsyn")
    if not args.inputs.exists():
        raise SystemExit(f"input directory {args.inputs} not found")

    qasms = sorted(p for p in args.inputs.glob("*.qasm"))
    if not qasms:
        raise SystemExit(f"no .qasm files found in {args.inputs}")

    rows: List[Row] = []
    for qasm in qasms:
        print(f"[bench] {qasm.name}", flush=True)
        rows.append(benchmark_one(args.qsyn, qasm, args.bqskit_opt_level, args.with_bqskit))

    lines = [
        "# CPF benchmark report",
        "",
        "Auto-generated by `scripts/cpf_bench.py`. Re-run after any pipeline change.",
        "",
        Row.header(),
        Row.sep(),
    ]
    for r in rows:
        lines.append(r.to_md())
    lines.append("")

    text = "\n".join(lines)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(text)
    print(f"[bench] wrote {len(rows)} rows to {args.out}")
    print(text)

    # Exit non-zero if any benchmark failed equivalence.
    any_broken = any(not r.cpf_equivalent for r in rows)
    return 1 if any_broken else 0


if __name__ == "__main__":
    sys.exit(main())
