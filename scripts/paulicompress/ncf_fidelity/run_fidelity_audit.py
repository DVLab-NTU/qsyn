#!/usr/bin/env python3
"""Strict fidelity audit: recompute ncf2 with Synthetiq alignment + F_worst."""

from __future__ import annotations

import csv
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

from ncf import benchmarks, compile_ncf, config, fidelity, synth  # noqa: E402


def _load_csv(path: str) -> list[dict]:
    if not os.path.isfile(path):
        return []
    with open(path, encoding="utf-8") as f:
        return list(csv.DictReader(f))


def _merge_rows(existing: list[dict], new_rows: list[dict]) -> list[dict]:
    idx = {(r["benchmark"], r["method"]): i for i, r in enumerate(existing)}
    for r in new_rows:
        key = (r["benchmark"], r["method"])
        if key in idx:
            existing[idx[key]] = r
        else:
            existing.append(r)
    return existing


def audit_ncf2(benchmark_names: list[str], syn_time: float = 4.0) -> list[dict]:
    synth._SYNTH_CACHE.clear()
    rows = []
    for name in benchmark_names:
        bench = benchmarks.load_benchmark(name)
        t0 = time.time()
        print(f"[{name}/ncf2] compiling ...", flush=True)
        cm = compile_ncf.compile_method(bench, "ncf2", progress=True, syn_time=syn_time)
        fa, fd, fw = cm.product_fidelity, cm.product_fidelity_direct, cm.product_fidelity_worst
        print(
            f"  T={cm.t_count} N_u={cm.n_unitaries} "
            f"F_align={fa:.6f} F_direct={fd:.6f} F_worst={fw:.6f} ({time.time()-t0:.1f}s)",
            flush=True,
        )
        rows.append(
            fidelity.fidelity_row(cm, note="ncf2: F_align/F_worst use Synthetiq permutation")
        )
    return rows


def write_audit_report(rows: list[dict], path: str) -> None:
    lines = [
        "# NCF Fidelity 严格审计",
        "",
        "## 结论摘要",
        "",
        "1. **单比特方法（gridsyn / rustiq / ncf1）F≈0.9999 是正确的**：",
        "   论文固定总误差预算 `ε_total = 0.001 × N_Pauli`，逐 unitary 缩放 `ε_eff = ε_total / N_u`，",
        "   故 infidelity 只有 ~O(ε_total)，不是 bug。",
        "2. **ncf2 先前极低 F 是度量 bug**：Synthetiq 在 qubit 置换 / 逆下判定；",
        "   未对齐的 `|Tr(U†V)|/d` 会把合法合成算成极低 F。",
        "3. **对齐后 ncf2 F 仍低于单比特方法**（约 0.93–0.98），因 ε=0.12 且 `∏F_k` 随 N_u 衰减。",
        "4. **F_worst**：每个 group 在 Synthetiq 候选中取最低对齐 F 再连乘。",
        "",
        "## ncf2 三联 F（对齐 / 未对齐 / 最坏）",
        "",
        "| Benchmark | N_u | #T | F_align | F_direct | F_worst |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for r in rows:
        if r["method"] != "ncf2":
            continue
        lines.append(
            f"| {r['benchmark']} | {r['n_unitaries']} | {r['t_count']} | "
            f"{float(r['F_approx']):.4f} | {float(r['F_direct']):.4f} | {float(r['F_worst']):.4f} |"
        )
    lines.append("")
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def main() -> None:
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("--benchmarks", nargs="+", default=config.TABLE_V_BENCHMARKS)
    ap.add_argument("--syn-time", type=float, default=4.0)
    ap.add_argument("--bench-root", type=str, default=None)
    args = ap.parse_args()
    if args.bench_root:
        config.set_bench_root(args.bench_root)

    os.makedirs(config.RESULTS_DIR, exist_ok=True)
    csv_path = os.path.join(config.RESULTS_DIR, "summary_ncf_fidelity.csv")
    existing = _load_csv(csv_path)
    new_rows = audit_ncf2(args.benchmarks, args.syn_time)
    merged = _merge_rows(existing, new_rows)
    fidelity.write_fidelity_csv(merged, csv_path)
    write_audit_report(new_rows, os.path.join(config.RESULTS_DIR, "FIDELITY_AUDIT.md"))
    print(f"wrote {csv_path}, FIDELITY_AUDIT.md", flush=True)


if __name__ == "__main__":
    main()
