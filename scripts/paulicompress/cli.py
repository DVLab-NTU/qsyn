#!/usr/bin/env python3
"""paulicompress — thesis Proposed Synthesis Flow + zero-sweep F-cost heuristic.

Core reproducible stack (Ch. 3–5):
  prepare → fast/slow CPF → zero-sweep (F*) → Gridsynth → qzq

Examples
--------
  make -j$(nproc)
  pip install -r scripts/paulicompress/requirements.txt

  # multi-F* zero-sweep query (prefix sums, no Gridsynth)
  python3 scripts/paulicompress/cli.py zero-sweep \\
      --pauli out/01_original_benchmarks/LiH/LiH.pauli \\
      --tiers 0.999 0.99 0.9

  # end-to-end proposed flow (default compress = zero-sweep)
  python3 scripts/paulicompress/cli.py all --bench LiH --fidelity 0.99 --eps 1e-3
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from flow import run_proposed_flow  # noqa: E402
from pauli_list import load_pauli_file  # noqa: E402
from prepare import prepare_benchmarks  # noqa: E402
from util import DEFAULT_QSYN_BIN  # noqa: E402
from zero_sweep import (  # noqa: E402
    DEFAULT_TIERS,
    build_budget,
    fidelity_to_l2_budget,
    plan_all_tiers,
    plan_min_pauli_rotation,
)


def _add_run_args(p: argparse.ArgumentParser) -> None:
    p.add_argument("--bench", required=True)
    p.add_argument("--bench-root", type=Path, default=Path("out/01_original_benchmarks"))
    p.add_argument("--out", type=Path, default=Path("out/flow"))
    p.add_argument("--fidelity", type=float, default=0.99)
    p.add_argument("--eps", type=float, default=1e-3)
    p.add_argument("--qsyn-bin", type=Path, default=DEFAULT_QSYN_BIN)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--skip-qzq", action="store_true")
    p.add_argument(
        "--compress",
        choices=["zero-sweep", "cpp-l2"],
        default="zero-sweep",
        help="default: thesis F-cost zero-sweep; cpp-l2 = Qsyn pauli-compress -l",
    )
    p.add_argument(
        "--l2",
        type=float,
        default=None,
        help="only for --compress cpp-l2 (default sqrt(-2 ln F*))",
    )


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(prog="paulicompress", description=__doc__)
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_prep = sub.add_parser("prepare", help="Paulihedral/lattice → 01_original layout")
    p_prep.add_argument("--out", type=Path, default=Path("out/01_original_benchmarks"))
    p_prep.add_argument("--bench", nargs="*", default=None)
    p_prep.add_argument("--dt", type=float, default=0.05)

    p_zs = sub.add_parser(
        "zero-sweep",
        help="Thesis zero-sweep: F-cost plan + optional multi-F* prefix queries",
    )
    p_zs.add_argument("--pauli", type=Path, required=True, help="Input .pauli list")
    p_zs.add_argument(
        "--tiers",
        nargs="*",
        type=float,
        default=list(DEFAULT_TIERS),
        help="Fidelity tiers (default 0.999 0.99 0.9)",
    )
    p_zs.add_argument("--fidelity", type=float, default=None, help="Single F* (+ write plan)")
    p_zs.add_argument("--out-pauli", type=Path, default=None, help="Write compressed .pauli")
    p_zs.add_argument("--json", type=Path, default=None, help="Write summary JSON")
    p_zs.add_argument("--t-per-rot", type=int, default=31)

    p_run = sub.add_parser("run", help="Run fast/slow proposed flow")
    p_run.add_argument("--mode", choices=["fast", "slow"], required=True)
    _add_run_args(p_run)

    p_all = sub.add_parser("all", help="prepare + run fast and/or slow")
    p_all.add_argument("--mode", choices=["fast", "slow", "both"], default="both")
    p_all.add_argument("--bench", required=True)
    p_all.add_argument("--out", type=Path, default=Path("out"))
    p_all.add_argument("--fidelity", type=float, default=0.99)
    p_all.add_argument("--eps", type=float, default=1e-3)
    p_all.add_argument("--qsyn-bin", type=Path, default=DEFAULT_QSYN_BIN)
    p_all.add_argument("--seed", type=int, default=42)
    p_all.add_argument("--skip-qzq", action="store_true")
    p_all.add_argument("--compress", choices=["zero-sweep", "cpp-l2"], default="zero-sweep")
    p_all.add_argument("--l2", type=float, default=None)
    p_all.add_argument("--dt", type=float, default=0.05)
    p_all.add_argument("--skip-prepare", action="store_true")

    args = parser.parse_args(argv)

    if args.cmd == "prepare":
        prepare_benchmarks(args.out, names=args.bench, dt=args.dt)
        print(f"[prepare] done → {args.out.resolve()}")
        return

    if args.cmd == "zero-sweep":
        pc = load_pauli_file(args.pauli)
        budget = build_budget(pc)
        print(f"[zero-sweep] {pc.name}: n_qubits={pc.n_qubits} n_nc_before={budget.n_before}")
        print(f"{'F*':>10} {'#rot':>8} {'F_exact':>12} {'L2':>12}")
        for f in args.tiers:
            n_aft, fid, l2 = budget.min_rot_at_fidelity(f)
            print(f"{f:10.6g} {n_aft:8d} {fid:12.6f} {l2:12.6g}")
        if args.fidelity is not None:
            plan = plan_min_pauli_rotation(pc, args.fidelity, t_per_rot=args.t_per_rot)
            print(
                f"[plan] F*={args.fidelity}: {plan.n_rot_before}->{plan.n_rot_after} "
                f"F_exact={plan.fidelity_exact:.6f} est_T={plan.est_t_count}"
            )
            if args.out_pauli:
                from pauli_list import write_pauli_file

                write_pauli_file(plan.compressed, args.out_pauli)
                print(f"[plan] wrote {args.out_pauli}")
            if args.json:
                args.json.write_text(
                    json.dumps(plan.summary_dict(), indent=2), encoding="utf-8"
                )
        elif args.tiers:
            plans = plan_all_tiers(pc, args.tiers, t_per_rot=args.t_per_rot)
            if args.json:
                payload = {str(k): v.summary_dict() for k, v in plans.items()}
                args.json.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        return

    if args.cmd == "run":
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
            compress=args.compress,
            l2_budget=args.l2,
        )
        return

    if args.cmd == "all":
        bench_root = Path(args.out) / "01_original_benchmarks"
        if not args.skip_prepare:
            prepare_benchmarks(bench_root, names=[args.bench], dt=args.dt)
        modes = ["fast", "slow"] if args.mode == "both" else [args.mode]
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
                compress=args.compress,
                l2_budget=args.l2,
            )
        return


if __name__ == "__main__":
    main()
