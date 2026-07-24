#!/usr/bin/env python3
"""paulicompress — thesis Proposed Synthesis Flow + research extras.

Main Proposed Flow (slide):
  Hamiltonian → [Phase Folding if slow] → Heuristic (zero-sweep)
              → Gridsynth → qzq → Clifford+T

  fast = no Phase Folding; slow = with Phase Folding.
  Methods A–E / HYBRID / RECURSIVE are optional research tools (--compress methods-ae).

Examples
--------
  make -j$(nproc)
  pip install -r scripts/paulicompress/requirements.txt

  # Proposed Flow (Heuristic default)
  python3 scripts/paulicompress/cli.py all --bench LiH --fidelity 0.99 --eps 1e-3
  python3 scripts/paulicompress/cli.py run --mode fast --bench LiH --fidelity 0.99
  python3 scripts/paulicompress/cli.py run --mode slow --bench LiH --fidelity 0.99

  # Composable stages (same Heuristic default)
  python3 scripts/paulicompress/cli.py pipeline \\
      --pauli out/01_original_benchmarks/LiH/LiH.pauli \\
      --compress zero-sweep --fidelity 0.99 --gridsynth --qzq
"""

from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from baselines import (  # noqa: E402
    DEFAULT_MOLECULE_BENCHES,
    load_stage,
)
from experiment_best import (  # noqa: E402
    DEFAULT_FIDELITY_TIERS,
    sweep_methods_ae,
)
from flow import COMPRESS_CHOICES, run_pipeline, run_proposed_flow  # noqa: E402
from hamiltonian_mappings import (  # noqa: E402
    MAPPING_NAMES,
    TRACK_B_MOLECULES,
    normalize_mapping,
    prepare_all_mappings,
)
from pauli_list import load_pauli_file  # noqa: E402
from phase_fold_compare import compare_suite  # noqa: E402
from prepare import prepare_benchmarks  # noqa: E402
from slow_compress import mapping_slow_compress, slow_cpf_then_compress  # noqa: E402
from util import DEFAULT_QSYN_BIN  # noqa: E402
from zero_sweep import (  # noqa: E402
    DEFAULT_TIERS,
    build_budget,
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
    p.add_argument("--skip-gridsynth", action="store_true")
    p.add_argument(
        "--compress",
        choices=list(COMPRESS_CHOICES),
        default="zero-sweep",
        help="Pauli compression (default: zero-sweep Heuristic; methods-ae is research)",
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
    p_all.add_argument("--skip-gridsynth", action="store_true")
    p_all.add_argument(
        "--compress",
        choices=list(COMPRESS_CHOICES),
        default="zero-sweep",
    )
    p_all.add_argument("--l2", type=float, default=None)
    p_all.add_argument("--dt", type=float, default=0.05)
    p_all.add_argument("--skip-prepare", action="store_true")

    p_pipe = sub.add_parser(
        "pipeline",
        help=(
            "Composable Proposed Flow stages: [Phase Folding] → compress → "
            "[Gridsynth] → [qzq]. Default compress = zero-sweep (Heuristic)."
        ),
    )
    p_pipe.add_argument("--pauli", type=Path, default=None)
    p_pipe.add_argument("--qasm", type=Path, default=None)
    p_pipe.add_argument("--out", type=Path, default=Path("out/pipeline"))
    p_pipe.add_argument(
        "--phase-fold",
        action="store_true",
        help="Enable Phase Folding (slow path; requires --qasm)",
    )
    p_pipe.add_argument(
        "--compress",
        choices=list(COMPRESS_CHOICES),
        default="zero-sweep",
        help="default: zero-sweep Heuristic (use methods-ae only for research)",
    )
    p_pipe.add_argument("--fidelity", type=float, default=0.99)
    p_pipe.add_argument("--l2", type=float, default=None)
    p_pipe.add_argument(
        "--gridsynth",
        action="store_true",
        help="Run Gridsynth after compression",
    )
    p_pipe.add_argument(
        "--qzq",
        action="store_true",
        help="Run qsyn qzq after Gridsynth (implies --gridsynth)",
    )
    p_pipe.add_argument("--eps", type=float, default=1e-3)
    p_pipe.add_argument("--qsyn-bin", type=Path, default=DEFAULT_QSYN_BIN)
    p_pipe.add_argument("--seed", type=int, default=42)
    p_pipe.add_argument("--name", type=str, default=None)

    p_ms = sub.add_parser(
        "methods-sweep",
        help="A–E / HYBRID / RECURSIVE grid + experiment_best at F* tiers",
    )
    src = p_ms.add_mutually_exclusive_group(required=True)
    src.add_argument("--pauli", type=Path, help="Arbitrary .pauli input")
    src.add_argument(
        "--bench",
        help="Load packaged baseline by name (requires --stage)",
    )
    p_ms.add_argument(
        "--stage",
        choices=["hamiltonian", "after_phase_fold"],
        default="hamiltonian",
        help="With --bench: which flow stage to load",
    )
    p_ms.add_argument("--bench-root", type=Path, default=None)
    p_ms.add_argument(
        "--tiers",
        nargs="*",
        type=float,
        default=list(DEFAULT_FIDELITY_TIERS),
    )
    p_ms.add_argument("--json", type=Path, default=None)
    p_ms.add_argument("--csv", type=Path, default=None, help="Write full sweep CSV")
    p_ms.add_argument("--no-adaptive-eps", action="store_true")

    p_cmp = sub.add_parser(
        "compare-phase-fold",
        help=(
            "Compare Methods A–E experiment_best on hamiltonian vs "
            "after_phase_fold (Phase Folding ablation)"
        ),
    )
    p_cmp.add_argument(
        "--bench",
        nargs="*",
        default=list(DEFAULT_MOLECULE_BENCHES),
        help=f"default: {' '.join(DEFAULT_MOLECULE_BENCHES)}",
    )
    p_cmp.add_argument(
        "--tiers",
        nargs="*",
        type=float,
        default=list(DEFAULT_FIDELITY_TIERS),
    )
    p_cmp.add_argument("--bench-root", type=Path, default=None)
    p_cmp.add_argument("--json", type=Path, default=None)

    p_pm = sub.add_parser(
        "prepare-mapping",
        help="Fermionic H → JW/BK/Parity Pauli lists (.pauli/.qasm)",
    )
    p_pm.add_argument(
        "--bench",
        nargs="*",
        default=list(TRACK_B_MOLECULES),
        help=f"default: {' '.join(TRACK_B_MOLECULES)}",
    )
    p_pm.add_argument(
        "--mapping",
        nargs="*",
        default=list(MAPPING_NAMES),
        help="default: jordan_wigner bravyi_kitaev parity (aliases: jw bk parity)",
    )
    p_pm.add_argument("--out", type=Path, default=Path("out/mapping_hamiltonian"))
    p_pm.add_argument("--dt", type=float, default=0.05)
    p_pm.add_argument(
        "--no-regen",
        action="store_true",
        help="fail if shipped ham_cache miss (do not call PySCF)",
    )

    p_mc = sub.add_parser(
        "mapping-compress",
        help=(
            "Mapping Hamiltonian → Phase Folding (slow CPF) → "
            "Methods A–E experiment_best (+ %% of raw)"
        ),
    )
    p_mc.add_argument("--bench", nargs="+", required=True)
    p_mc.add_argument(
        "--mapping",
        nargs="*",
        default=list(MAPPING_NAMES),
        help="default: all three mappings",
    )
    p_mc.add_argument("--out", type=Path, default=Path("out/mapping_compress"))
    p_mc.add_argument(
        "--tiers",
        nargs="*",
        type=float,
        default=list(DEFAULT_FIDELITY_TIERS),
    )
    p_mc.add_argument("--dt", type=float, default=0.05)
    p_mc.add_argument("--qsyn-bin", type=Path, default=DEFAULT_QSYN_BIN)
    p_mc.add_argument(
        "--skip-phase-fold",
        action="store_true",
        help="compress Hamiltonian list directly (no slow CPF)",
    )
    p_mc.add_argument("--no-regen", action="store_true")
    p_mc.add_argument("--json", type=Path, default=None)

    p_sc = sub.add_parser(
        "slow-compress",
        help="Existing .pauli+.qasm → Phase Folding → Methods A–E",
    )
    p_sc.add_argument("--pauli", type=Path, required=True)
    p_sc.add_argument("--qasm", type=Path, required=True)
    p_sc.add_argument("--out", type=Path, default=Path("out/slow_compress"))
    p_sc.add_argument(
        "--tiers",
        nargs="*",
        type=float,
        default=list(DEFAULT_FIDELITY_TIERS),
    )
    p_sc.add_argument("--qsyn-bin", type=Path, default=DEFAULT_QSYN_BIN)
    p_sc.add_argument("--skip-phase-fold", action="store_true")
    p_sc.add_argument("--json", type=Path, default=None)

    p_nf = sub.add_parser(
        "ncf-fidelity",
        help=(
            "NCF pathway synthesis-fidelity audit: per fused unitary "
            "F_k=|Tr(U†V)|/d then product ∏ F_k (see ncf_fidelity/README.md)"
        ),
    )
    p_nf.add_argument(
        "--bench",
        nargs="+",
        default=["LiH"],
        help="Benchmark names (default: LiH)",
    )
    p_nf.add_argument(
        "--all-table-v",
        action="store_true",
        help="All Table V benchmarks (excludes H2S/CO2 for ncf2 paper set)",
    )
    p_nf.add_argument(
        "--methods",
        nargs="+",
        default=["gridsyn", "ncf1"],
        choices=["gridsyn", "rustiq_trasyn", "ncf1", "ncf2"],
        help="Default: gridsyn ncf1 (add ncf2 if Synthetiq is available)",
    )
    p_nf.add_argument(
        "--bench-root",
        type=Path,
        default=None,
        help="Dir with <Bench>/<Bench>.pauli (else reproduction bundle)",
    )
    p_nf.add_argument("--syn-time", type=float, default=8.0)
    p_nf.add_argument("--limit-groups", type=int, default=None)
    p_nf.add_argument("--lih-exact", action="store_true")
    p_nf.add_argument(
        "--emit-tables",
        action="store_true",
        help="Also emit TABLE_V_* markdown (optional CT CSVs)",
    )

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

    if args.cmd == "methods-sweep":
        if args.pauli is not None:
            pc = load_pauli_file(args.pauli)
        else:
            pc = load_stage(args.bench, args.stage, bench_root=args.bench_root)
        sweep = sweep_methods_ae(pc, adaptive_eps=not args.no_adaptive_eps)
        print(
            f"[methods-sweep] {pc.name}: n_qubits={pc.n_qubits} "
            f"n_nc_before={sweep.n_rot_before} n_points={len(sweep.points)}"
        )
        print(f"{'F*':>10} {'#rot':>8} {'method':>12} {'setting':>24} {'F_approx':>12}")
        table = sweep.experiment_best_table(args.tiers)
        for f, best in table.items():
            if best is None:
                print(f"{f:10.6g} {'—':>8} {'—':>12} {'—':>24} {'—':>12}")
            else:
                print(
                    f"{f:10.6g} {best.n_rot:8d} {best.method:>12} "
                    f"{best.setting:>24} {best.approx_fidelity:12.6f}"
                )
        if args.csv:
            args.csv.parent.mkdir(parents=True, exist_ok=True)
            with open(args.csv, "w", encoding="utf-8", newline="") as f:
                w = csv.DictWriter(
                    f,
                    fieldnames=[
                        "method",
                        "setting",
                        "n_rot_before",
                        "n_rot_after",
                        "approx_fidelity",
                        "l1_err",
                        "l2_err",
                        "note",
                    ],
                )
                w.writeheader()
                for p in sweep.points:
                    w.writerow(p.summary_dict())
            print(f"[methods-sweep] wrote CSV → {args.csv}")
        if args.json:
            payload = {
                "name": sweep.name,
                "n_qubits": sweep.n_qubits,
                "n_rot_before": sweep.n_rot_before,
                "experiment_best": {
                    str(k): (None if v is None else v.summary_dict())
                    for k, v in table.items()
                },
                "n_points": len(sweep.points),
            }
            args.json.write_text(json.dumps(payload, indent=2), encoding="utf-8")
            print(f"[methods-sweep] wrote JSON → {args.json}")
        return

    if args.cmd == "compare-phase-fold":
        rows = compare_suite(
            args.bench, tiers=args.tiers, bench_root=args.bench_root
        )
        hdr = (
            f"{'bench':10} {'#ham':>7} {'#fold':>7} | "
            + " | ".join(f"F≥{f:g} ham/fold" for f in args.tiers)
        )
        print(hdr)
        print("-" * len(hdr))
        for row in rows:
            cells = []
            for f in args.tiers:
                bh = row.hamiltonian.experiment_best.get(float(f))
                bf = row.after_phase_fold.experiment_best.get(float(f))
                nh = "—" if bh is None else str(bh.n_rot)
                nf = "—" if bf is None else str(bf.n_rot)
                cells.append(f"{nh}/{nf}")
            print(
                f"{row.bench:10} {row.n_rot_hamiltonian:7d} "
                f"{row.n_rot_after_phase_fold:7d} | " + " | ".join(f"{c:>14}" for c in cells)
            )
        if args.json:
            payload = [r.summary_dict() for r in rows]
            args.json.write_text(json.dumps(payload, indent=2), encoding="utf-8")
            print(f"[compare-phase-fold] wrote JSON → {args.json}")
        return

    if args.cmd == "prepare-mapping":
        maps = [normalize_mapping(m) for m in args.mapping]
        written = prepare_all_mappings(
            args.out,
            benches=args.bench,
            mappings=maps,
            dt=args.dt,
            allow_regen=not args.no_regen,
        )
        print(f"[prepare-mapping] wrote {len(written)} dirs under {args.out.resolve()}")
        return

    if args.cmd == "mapping-compress":
        maps = [normalize_mapping(m) for m in args.mapping]
        results = []
        print(
            f"{'name':28} {'#raw':>6} {'#fold':>6} | "
            + " | ".join(f"F≥{f:g} #/%" for f in args.tiers)
        )
        for bench in args.bench:
            for mapping in maps:
                res = mapping_slow_compress(
                    bench,
                    mapping,
                    out_root=args.out,
                    tiers=args.tiers,
                    dt=args.dt,
                    qsyn_bin=args.qsyn_bin,
                    skip_phase_fold=args.skip_phase_fold,
                    allow_regen=not args.no_regen,
                )
                results.append(res)
                cells = []
                for f in args.tiers:
                    best = res.tiers.get(float(f))
                    pct = res.remaining_pct(float(f))
                    if best is None or pct is None:
                        cells.append("—")
                    else:
                        cells.append(f"{best.n_rot}/{pct:.1f}%")
                print(
                    f"{res.name:28} {res.n_rot_raw:6d} {res.n_rot_after_phase_fold:6d} | "
                    + " | ".join(f"{c:>12}" for c in cells)
                )
        if args.json:
            args.json.write_text(
                json.dumps([r.summary_dict() for r in results], indent=2),
                encoding="utf-8",
            )
            print(f"[mapping-compress] wrote JSON → {args.json}")
        return

    if args.cmd == "slow-compress":
        res = slow_cpf_then_compress(
            pauli_path=args.pauli,
            qasm_path=args.qasm,
            out_dir=args.out,
            tiers=args.tiers,
            qsyn_bin=args.qsyn_bin,
            skip_phase_fold=args.skip_phase_fold,
        )
        print(
            f"[slow-compress] {res.name}: raw={res.n_rot_raw} "
            f"after_phase_fold={res.n_rot_after_phase_fold}"
        )
        print(f"{'F*':>10} {'#rot':>8} {'%raw':>8} {'method':>12} {'setting':>24}")
        for f, best in res.tiers.items():
            pct = res.remaining_pct(f)
            if best is None:
                print(f"{f:10.6g} {'—':>8} {'—':>8}")
            else:
                print(
                    f"{f:10.6g} {best.n_rot:8d} {pct:7.2f}% "
                    f"{best.method:>12} {best.setting:>24}"
                )
        if args.json:
            args.json.write_text(
                json.dumps(res.summary_dict(), indent=2), encoding="utf-8"
            )
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
            gridsynth=not args.skip_gridsynth,
        )
        return

    if args.cmd == "pipeline":
        do_qzq = bool(args.qzq)
        do_gs = bool(args.gridsynth) or do_qzq
        run_pipeline(
            out_dir=args.out,
            pauli_path=args.pauli,
            qasm_path=args.qasm,
            phase_fold=args.phase_fold,
            compress=args.compress,
            fidelity=args.fidelity,
            l2_budget=args.l2,
            gridsynth=do_gs,
            qzq=do_qzq,
            epsilon=args.eps,
            qsyn_bin=args.qsyn_bin,
            seed=args.seed,
            name=args.name,
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
                gridsynth=not args.skip_gridsynth,
            )
        return

    if args.cmd == "ncf-fidelity":
        ncf_root = HERE / "ncf_fidelity"
        if str(ncf_root) not in sys.path:
            sys.path.insert(0, str(ncf_root))
        from run_ncf_repro import run as run_ncf_fidelity  # noqa: E402
        from ncf import config as ncf_config  # noqa: E402

        if args.bench_root is not None:
            ncf_config.set_bench_root(str(args.bench_root))
        names = (
            list(ncf_config.TABLE_V_BENCHMARKS)
            if args.all_table_v
            else list(args.bench)
        )
        run_ncf_fidelity(
            names,
            list(args.methods),
            args.lih_exact,
            args.syn_time,
            args.limit_groups,
            emit_tables=args.emit_tables,
        )
        return


if __name__ == "__main__":
    main()
