#!/usr/bin/env python3
"""Compare CPF fold strategies on small benchmarks (rotation count after fold).

Runs `qcir cpf-optimize --skip-u3cx` with different `--fold-strategy` and
`--rounds`, reports tableau rotation counts from qsyn stdout.

See docs/cpf_strategy_bench.md for interpretation.

Example:
  python scripts/cpf_strategy_bench.py --qsyn build/qsyn \\
      --out docs/cpf_strategy_bench_results.md
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

_ROT_RE = re.compile(
    r"cpf-optimize: strategy (\S+) rotations (\d+) -> (\d+)",
    re.IGNORECASE,
)


@dataclass(frozen=True)
class Strategy:
    label: str
    fold_strategy: str
    rounds: int = 1
    extra_flags: str = ""


STRATEGIES: List[Strategy] = [
    Strategy("global_r1", "global", 1),
    Strategy("staq_r1", "staq", 1),
    Strategy("no_prune_r1", "no_prune", 1),
    Strategy("dag_r1", "dag", 1),
    Strategy("matching_r1", "matching", 1),
    Strategy("dag_r3", "dag", 3),
    Strategy("matching_r3", "matching", 3),
]

# Paper-table monsters: only compare global vs dag (1 round).
STRATEGIES_LITE: List[Strategy] = [
    Strategy("global_r1", "global", 1),
    Strategy("dag_r1", "dag", 1),
    Strategy("matching_r1", "matching", 1),
]

LARGE_BENCH = frozenset({"rd73_252", "urf2_277"})
# Skip 3-round retranspile on medium/heavy circuits (very slow after first fold).
NO_R3_BENCH = frozenset({
    "adder_n64", "adder_n118", "cm42a_207", "cm82a_208", "cm152a_212",
    "hwb5_53", "ham7_104", "rd53_251", "rd73_252", "urf2_277",
})
# Very heavy: only global / dag / matching (1 round); skip staq/no_prune ablations.
HEAVY_LITE_BENCH = frozenset({"adder_n118", "rd73_252", "urf2_277"})


def _resolve_path(qsyn_root: Path, spec: str) -> Path:
    p = Path(spec)
    if p.is_absolute():
        return p
    if spec.startswith("CPF/"):
        return (qsyn_root.parent / spec).resolve()
    return (qsyn_root / spec).resolve()


def _load_manifest(qsyn_root: Path, manifest: Path) -> List[Tuple[str, Path]]:
    out: List[Tuple[str, Path]] = []
    for line in manifest.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.lower().startswith("skip "):
            continue
        name, _, path_spec = line.partition(" ")
        if name.lower() == "skip":
            continue
        out.append((name, _resolve_path(qsyn_root, path_spec)))
    return out


def _run_qsyn(qsyn: Path, commands: List[str], timeout: int) -> str:
    cmd = [str(qsyn), "-q", "-c", ";".join(commands)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    if r.returncode != 0:
        raise RuntimeError(f"qsyn failed: {cmd}\n{r.stdout}\n{r.stderr}")
    return r.stdout + r.stderr


def _classify_qasm(path: Path) -> str:
    try:
        from qiskit import QuantumCircuit
    except ImportError:
        return "unknown"
    qc = QuantumCircuit.from_qasm_file(str(path))
    arb = {"u", "u3", "u2", "rx", "ry", "rz", "p", "u1"}
    cliff = {"h", "x", "y", "z", "s", "sdg", "t", "tdg", "cx", "cnot", "cz", "swap"}
    n_arb = n_cliff = 0
    for ins, qargs, _ in qc.data:
        name = ins.name
        if name in arb:
            n_arb += 1
        elif name in cliff or name.startswith("c"):
            n_cliff += 1
    if n_arb > 0 and n_cliff > 0:
        return "mixed"
    if n_arb > 0:
        return "arbitrary"
    return "clifford"


def _cpf_rotations(qsyn: Path, src: Path, strat: Strategy, timeout: int) -> Tuple[int, int]:
    cmds = [
        f"qcir read {src}",
        (f"qcir cpf-optimize --skip-u3cx --fold-strategy {strat.fold_strategy} "
         f"--rounds {strat.rounds} -r {strat.extra_flags}").strip(),
        "quit -f",
    ]
    log = _run_qsyn(qsyn, cmds, timeout)
    m = _ROT_RE.search(log)
    if not m:
        raise RuntimeError(f"no rotation line in log for {src.name} / {strat.label}\n{log[-2000:]}")
    return int(m.group(2)), int(m.group(3))


def _try_pauliopt(path: Path) -> Optional[int]:
    """Best-effort PauliOpt gate count; None if package missing."""
    try:
        from qiskit import QuantumCircuit, transpile
        from qiskit.qasm2 import loads
    except ImportError:
        return None
    try:
        from pauliopt import Circuit
        from pauliopt.phase import OptimizedPhaseCircuit, PhaseCircuit, pi, Z
    except ImportError:
        return None
    try:
        text = path.read_text()
        qc = QuantumCircuit.from_qasm_file(str(path))
        # PauliOpt demo path: only for small rz-native sketches; full QASM import
        # is not always supported — skip non-trivial circuits.
        if path.name != "bench_2q_continuous.qasm":
            return None
        n = qc.num_qubits
        circ = PhaseCircuit(n)
        # Placeholder: user installs pauliopt for full comparison.
        _ = (Circuit, OptimizedPhaseCircuit, pi, Z, circ, loads, transpile)
        return None
    except Exception:
        return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--qsyn", type=Path, default=Path("build/qsyn"))
    ap.add_argument("--out", type=Path, default=Path("docs/cpf_strategy_bench_results.md"))
    ap.add_argument(
        "--inputs",
        type=Path,
        nargs="*",
        default=None,
        help="QASM files (default: testcase/benchmark + pr4 h_sandwich + qft_3 + qft_4)",
    )
    ap.add_argument(
        "--manifest",
        type=Path,
        default=None,
        help="Manifest: 'name path' per line (e.g. benchmark/jku_table17.manifest).",
    )
    ap.add_argument("--timeout", type=int, default=600,
                    help="Per-qsyn-invocation timeout in seconds (default 600).")
    ap.add_argument("--checkpoint", type=Path, default=None,
                    help="JSON checkpoint (resume + append results).")
    args = ap.parse_args()

    if not args.qsyn.exists():
        print(f"build qsyn first: {args.qsyn}", file=sys.stderr)
        return 1

    root = Path(__file__).resolve().parents[1]
    bench_items: List[Tuple[str, Path]] = []

    if args.manifest:
        man = args.manifest if args.manifest.is_absolute() else root / args.manifest
        if not man.exists():
            raise SystemExit(f"manifest not found: {man}")
        bench_items = _load_manifest(root, man)
    elif args.inputs:
        bench_items = [(Path(p).name, Path(p)) for p in args.inputs]
    else:
        files = sorted((root / "testcase/benchmark/qasm").glob("*.qasm"))
        extra = [
            root / "testcase/pr4_global_fold/qasm/h_sandwich_pair.qasm",
            root / "benchmark/SABRE/small/qft_4.qasm",
        ]
        for p in extra:
            if p.exists():
                files.append(p)
        bench_items = [(p.name, p) for p in files]

    all_labels: List[str] = []
    results: Dict[str, Dict[str, Tuple[int, int]]] = {}
    kinds: Dict[str, str] = {}

    if args.checkpoint and args.checkpoint.exists():
        ck = json.loads(args.checkpoint.read_text())
        kinds = ck.get("kinds", {})
        for k, v in ck.get("results", {}).items():
            results[k] = {s: tuple(p) for s, p in v.items()}
        all_labels = ck.get("all_labels", [])
        print(f"[bench] loaded checkpoint {len(results)} circuits", flush=True)

    def _pick_strategies(circuit: str) -> List[Strategy]:
        if circuit in HEAVY_LITE_BENCH or circuit in LARGE_BENCH:
            return STRATEGIES_LITE
        if circuit in NO_R3_BENCH:
            return [s for s in STRATEGIES if s.rounds == 1]
        return STRATEGIES

    def _timeout_for(circuit: str) -> int:
        if circuit in HEAVY_LITE_BENCH:
            return max(args.timeout, 2400)
        if circuit in {"adder_n64", "rd53_251", "cm152a_212", "hwb5_53"}:
            return max(args.timeout, 1200)
        return args.timeout

    def _save_checkpoint() -> None:
        if not args.checkpoint:
            return
        args.checkpoint.parent.mkdir(parents=True, exist_ok=True)
        args.checkpoint.write_text(json.dumps({
            "kinds": kinds,
            "results": {k: {s: list(p) for s, p in v.items()} for k, v in results.items()},
            "all_labels": all_labels,
        }, indent=2))

    for name, path in bench_items:
        if not path.exists():
            print(f"[bench] {name}: MISSING {path}", flush=True)
            continue
        key = name
        needed = _pick_strategies(key)
        if key in results and all(
            s.label in results[key] and results[key][s.label][1] >= 0 for s in needed
        ):
            print(f"[bench] {key}: skip (checkpoint complete)", flush=True)
            continue
        kinds[key] = _classify_qasm(path)
        if key not in results:
            results[key] = {}
        strategies = _pick_strategies(key)
        for s in strategies:
            if s.label not in all_labels:
                all_labels.append(s.label)
        print(f"[bench] {key} ({kinds[key]}) {path}", flush=True)
        for s in strategies:
            if s.label in results[key] and results[key][s.label][1] >= 0:
                print(f"  {s.label}: cached {results[key][s.label][0]} -> {results[key][s.label][1]}",
                      flush=True)
                continue
            try:
                before, after = _cpf_rotations(args.qsyn, path, s, _timeout_for(key))
            except (RuntimeError, subprocess.TimeoutExpired) as exc:
                print(f"  {s.label}: SKIP ({exc})", flush=True)
                results[key][s.label] = (-1, -1)
            else:
                results[key][s.label] = (before, after)
                print(f"  {s.label}: {before} -> {after}", flush=True)
            _save_checkpoint()

    lines = [
        "# CPF fold strategy comparison",
        "",
        "Generated by `scripts/cpf_strategy_bench.py`. "
        "Metric: **tableau Pauli rotations** after trace_replay + fold "
        "(--skip-u3cx, no full_optimize).",
        "",
        "| circuit | kind | " + " | ".join(all_labels) + " | best |",
        "|" + "|".join(["---"] * (3 + len(all_labels))) + "|",
    ]

    for key in sorted(results.keys()):
        cols = []
        best_after = 10**9
        best_label = "-"
        for label in all_labels:
            pair = results[key].get(label, (-1, -1))
            b, a = pair
            cols.append("—" if a < 0 else f"{b}→{a}")
            if a >= 0 and a < best_after:
                best_after = a
                best_label = label
        if best_after == 10**9:
            best_label = "—"
            best_after = "—"
        lines.append(
            f"| `{key}` | {kinds[key]} | " + " | ".join(cols) + f" | **{best_label}** ({best_after}) |"
        )

    lines += [
        "",
        "## Notes",
        "",
        "- **dag** = staq + greedy graph merge + global_prune + global_fold.",
        "- **matching** = same but max matching on Cole-legal merge edges.",
        "- **global** = propagation_merge only (`global_fold`).",
        "- **staq** / **no_prune** = ablations of dag_fold layers.",
        "- **dag_r3** / **matching_r3** = retranspile loop (fold→QCir→re-enter).",
        "- PauliOpt not run automatically (optional dep); different IR / may add CX.",
        "",
    ]

    text = "\n".join(lines)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(text)
    print(f"\nWrote {args.out}\n{text}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
