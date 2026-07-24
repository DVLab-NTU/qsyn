#!/usr/bin/env python3
"""Driver: NCF pathway synthesis + product process-fidelity audit.

Compiles each benchmark under gridsyn / rustiq_trasyn / ncf1 / ncf2 and reports
``F = ∏_k |Tr(U_k† V_k)| / d_k`` (aligned / direct / worst for ncf2).

Usage
-----
    python3 -m ncf_fidelity.run_ncf_repro --benchmarks LiH --methods gridsyn ncf1
    # or via lab CLI:
    python3 scripts/paulicompress/cli.py ncf-fidelity --bench LiH --methods gridsyn ncf1
"""

from __future__ import annotations

import argparse
import os
import sys
import time
from typing import List, Optional

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

from ncf import (  # noqa: E402
    benchmarks,
    compile_ncf,
    config,
    fidelity,
    metrics,
)

DEFAULT_METHODS = ["gridsyn", "rustiq_trasyn", "ncf1", "ncf2"]


def run(
    benchmark_names: List[str],
    methods: List[str],
    lih_exact: bool,
    syn_time: float,
    limit_groups: Optional[int],
    *,
    emit_tables: bool = False,
) -> None:
    tablev_rows = []
    fidelity_rows = []
    circuits_dir = config.CIRCUITS_DIR
    os.makedirs(circuits_dir, exist_ok=True)
    os.makedirs(config.RESULTS_DIR, exist_ok=True)

    csv_path = os.path.join(config.RESULTS_DIR, "summary_ncf_fidelity.csv")
    existing_fidelity = fidelity.read_fidelity_csv(csv_path)
    tablev_path = os.path.join(config.RESULTS_DIR, "summary_ncf_tablev.csv")
    existing_tablev = metrics.read_tablev_csv(tablev_path)

    for name in benchmark_names:
        bench = benchmarks.load_benchmark(name)
        for method in methods:
            t0 = time.time()
            print(f"[{name}/{method}] compiling ...", flush=True)
            try:
                cm = compile_ncf.compile_method(
                    bench,
                    method,
                    limit_groups=limit_groups,
                    progress=True,
                    syn_time=syn_time,
                )
            except Exception as exc:  # noqa: BLE001
                print(f"  FAILED: {exc}", flush=True)
                continue
            elapsed = time.time() - t0

            mrow = metrics.metrics_row(cm)
            tablev_rows.append(mrow)
            ref = metrics.paper_reference(name, method)
            ref_s = f" paper_T={ref[0]}" if ref else ""
            print(
                f"  T={cm.t_count} Td={mrow['t_depth']} Clifford={cm.clifford_count} "
                f"N_u={cm.n_unitaries} eps={cm.eps:.5g} "
                f"F_approx={cm.product_fidelity:.6f}{ref_s}  ({elapsed:.1f}s)",
                flush=True,
            )

            f_exact = None
            note = ""
            if method in ("gridsyn", "ncf1", "ncf2"):
                qpath = os.path.join(circuits_dir, f"{name}_{method}.qasm")
                with open(qpath, "w", encoding="utf-8") as f:
                    f.write(compile_ncf.emit_qasm(cm))
                if lih_exact and name == "LiH":
                    print("  computing exact LiH statevector fidelity ...", flush=True)
                    te = time.time()
                    try:
                        f_exact = fidelity.lih_exact_fidelity(cm, qpath)
                        print(
                            f"  F_exact={f_exact:.6f}  ({time.time() - te:.1f}s)",
                            flush=True,
                        )
                    except Exception as exc:  # noqa: BLE001
                        note = f"exact-failed:{exc}"
                        print(f"  exact fidelity failed: {exc}", flush=True)
            else:
                note = "rustiq: fidelity = per-rotation gridsynth proxy at same eps"

            fidelity_rows.append(fidelity.fidelity_row(cm, f_exact, note))

    merged_tablev = metrics.merge_tablev_rows(existing_tablev, tablev_rows)
    metrics.write_tablev_csv(merged_tablev, tablev_path)
    merged_fidelity = fidelity.merge_fidelity_rows(existing_fidelity, fidelity_rows)
    fidelity.write_fidelity_csv(merged_fidelity, csv_path)
    if emit_tables:
        try:
            from ncf import tablev_fidelity  # noqa: E402

            tablev_fidelity.emit_all(csv_path)
        except Exception as exc:  # noqa: BLE001
            print(f"[warn] tablev markdown emit skipped: {exc}", flush=True)
    print(
        f"\nwrote {config.RESULTS_DIR}/summary_ncf_tablev.csv, "
        f"summary_ncf_fidelity.csv",
        flush=True,
    )


def main(argv: Optional[List[str]] = None) -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--benchmarks", nargs="+", default=["LiH"])
    ap.add_argument("--all", action="store_true", help="all Table V benchmarks")
    ap.add_argument("--methods", nargs="+", default=DEFAULT_METHODS)
    ap.add_argument("--lih-exact", action="store_true")
    ap.add_argument("--syn-time", type=float, default=8.0, help="Synthetiq seconds/group")
    ap.add_argument("--limit-groups", type=int, default=None)
    ap.add_argument(
        "--bench-root",
        type=str,
        default=None,
        help="Directory with <Bench>/<Bench>.pauli (else reproduction bundle)",
    )
    ap.add_argument(
        "--emit-tables",
        action="store_true",
        help="Also emit TABLE_V_* markdown (needs optional CT CSVs)",
    )
    args = ap.parse_args(argv)

    if args.bench_root:
        config.set_bench_root(args.bench_root)
    names = config.TABLE_V_BENCHMARKS if args.all else args.benchmarks
    run(
        names,
        args.methods,
        args.lih_exact,
        args.syn_time,
        args.limit_groups,
        emit_tables=args.emit_tables,
    )


if __name__ == "__main__":
    main()
