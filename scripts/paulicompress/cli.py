#!/usr/bin/env python3
"""paulicompress — Proposed Synthesis Flow (fast / slow) for lab use.

Examples
--------
  # 0) build qsyn (once)
  make -j$(nproc)

  # 1) Paulihedral / lattice → 01_original_benchmarks
  python3 scripts/paulicompress/cli.py prepare \\
      --out out/01_original_benchmarks --bench LiH Ising-2D-30

  # 2a) fast:  Hamiltonian → PauliCompress → Gridsynth → qzq
  python3 scripts/paulicompress/cli.py run --mode fast \\
      --bench LiH --bench-root out/01_original_benchmarks \\
      --fidelity 0.99 --eps 1e-3 --out out/flow

  # 2b) slow:  Hamiltonian → to-zyz → PauliDAG → PauliCompress → Gridsynth → qzq
  python3 scripts/paulicompress/cli.py run --mode slow \\
      --bench LiH --bench-root out/01_original_benchmarks \\
      --fidelity 0.99 --eps 1e-3 --out out/flow

  # 3) one-shot (prepare + both modes)
  python3 scripts/paulicompress/cli.py all --bench LiH --fidelity 0.99
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from flow import run_proposed_flow  # noqa: E402
from prepare import prepare_benchmarks  # noqa: E402
from util import DEFAULT_QSYN_BIN, fidelity_to_l2_budget  # noqa: E402


def _add_common_run_args(p: argparse.ArgumentParser) -> None:
    p.add_argument("--bench", required=True, help="Benchmark name, e.g. LiH")
    p.add_argument(
        "--bench-root",
        type=Path,
        default=Path("out/01_original_benchmarks"),
        help="Directory that contains <Bench>/<Bench>.pauli(.qasm)",
    )
    p.add_argument("--out", type=Path, default=Path("out/flow"), help="Output root")
    p.add_argument(
        "--fidelity",
        type=float,
        default=0.99,
        help="Target process fidelity F* for heuristic PauliCompress (default 0.99)",
    )
    p.add_argument(
        "--l2",
        type=float,
        default=None,
        help="Override L2 budget for `tableau optimize pauli-compress -l` "
        "(default: sqrt(-2 ln F*))",
    )
    p.add_argument("--eps", type=float, default=1e-3, help="Gridsynth ε (default 1e-3)")
    p.add_argument("--qsyn-bin", type=Path, default=DEFAULT_QSYN_BIN)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--skip-qzq", action="store_true", help="Stop after Gridsynth")


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(
        prog="paulicompress",
        description="Proposed Synthesis Flow: prepare benchmarks + fast/slow Clifford+T",
    )
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_prep = sub.add_parser("prepare", help="Paulihedral/lattice → 01_original_benchmarks")
    p_prep.add_argument("--out", type=Path, default=Path("out/01_original_benchmarks"))
    p_prep.add_argument("--bench", nargs="*", default=None)
    p_prep.add_argument("--dt", type=float, default=0.05)

    p_run = sub.add_parser("run", help="Run fast or slow flow on one benchmark")
    p_run.add_argument("--mode", choices=["fast", "slow"], required=True)
    _add_common_run_args(p_run)

    p_all = sub.add_parser("all", help="prepare + run fast and/or slow")
    p_all.add_argument(
        "--mode",
        choices=["fast", "slow", "both"],
        default="both",
        help="Which synthesis route(s) to run (default both)",
    )
    p_all.add_argument("--bench", required=True)
    p_all.add_argument("--out", type=Path, default=Path("out"))
    p_all.add_argument("--fidelity", type=float, default=0.99)
    p_all.add_argument("--l2", type=float, default=None)
    p_all.add_argument("--eps", type=float, default=1e-3)
    p_all.add_argument("--qsyn-bin", type=Path, default=DEFAULT_QSYN_BIN)
    p_all.add_argument("--seed", type=int, default=42)
    p_all.add_argument("--skip-qzq", action="store_true")
    p_all.add_argument("--dt", type=float, default=0.05)
    p_all.add_argument(
        "--skip-prepare",
        action="store_true",
        help="Reuse existing out/01_original_benchmarks",
    )

    args = parser.parse_args(argv)

    if args.cmd == "prepare":
        prepare_benchmarks(args.out, names=args.bench, dt=args.dt)
        print(f"[prepare] done → {args.out.resolve()}")
        return

    if args.cmd == "run":
        budget = args.l2
        if budget is None:
            budget = fidelity_to_l2_budget(args.fidelity)
            print(f"[run] F*={args.fidelity} → L2 budget {budget:.6g}")
        run_proposed_flow(
            mode=args.mode,
            bench=args.bench,
            bench_root=args.bench_root,
            out_root=args.out,
            fidelity=args.fidelity,
            epsilon=args.eps,
            qsyn_bin=args.qsyn_bin,
            seed=args.seed,
            skip_qzq=args.skip_qzq,
            l2_budget=budget,
        )
        return

    if args.cmd == "all":
        bench_root = Path(args.out) / "01_original_benchmarks"
        if not args.skip_prepare:
            prepare_benchmarks(bench_root, names=[args.bench], dt=args.dt)
        modes = ["fast", "slow"] if args.mode == "both" else [args.mode]
        budget = args.l2 if args.l2 is not None else fidelity_to_l2_budget(args.fidelity)
        print(f"[all] F*={args.fidelity} → L2 budget {budget:.6g}")
        for mode in modes:
            run_proposed_flow(
                mode=mode,
                bench=args.bench,
                bench_root=bench_root,
                out_root=Path(args.out) / "flow",
                fidelity=args.fidelity,
                epsilon=args.eps,
                qsyn_bin=args.qsyn_bin,
                seed=args.seed,
                skip_qzq=args.skip_qzq,
                l2_budget=budget,
            )
        return


if __name__ == "__main__":
    main()
