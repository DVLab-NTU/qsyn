"""Gate metrics (T-count, T-depth, Clifford count) and Table V comparison."""

from __future__ import annotations

import csv
import os
from typing import Dict, Iterable, List, Tuple

from . import config
from .compile_ncf import CompiledMethod


def iter_full_ops(cm: CompiledMethod) -> Iterable[Tuple[str, List[int]]]:
    """Yield (gate, global-qubits) for the whole synthesised circuit."""
    inv = {"h": "h", "s": "sdg", "sdg": "s", "x": "x", "t": "tdg", "tdg": "t",
           "y": "y", "z": "z", "cx": "cx", "cz": "cz"}
    for g in cm.groups:
        for name, qs in g.clifford:
            yield name, list(qs)
        for name, lqs in g.synth.ops:
            yield name, [g.pivots[i] for i in lqs]
        for name, qs in reversed(g.clifford):
            yield inv[name], list(qs)


def t_depth_full(cm: CompiledMethod) -> int:
    """ASAP-scheduled T-depth over the whole circuit."""
    time_q = [0] * cm.n_qubits
    t_layers = set()
    for name, qs in iter_full_ops(cm):
        tq = max(time_q[q] for q in qs) + 1
        for q in qs:
            time_q[q] = tq
        if name in ("t", "tdg"):
            t_layers.add(tq)
    return len(t_layers)


def metrics_row(cm: CompiledMethod) -> Dict[str, object]:
    return {
        "benchmark": cm.benchmark,
        "method": cm.method,
        "n_qubits": cm.n_qubits,
        "n_paulis": cm.n_paulis,
        "n_unitaries": cm.n_unitaries,
        "eps": f"{cm.eps:.6g}",
        "t_count": cm.t_count,
        "t_depth": t_depth_full(cm),
        "clifford_count": cm.clifford_count,
    }


# Column index of each method in PAPER_TABLE_V tuples.
_PAPER_COL = {"gridsyn": 0, "rustiq_trasyn": 1, "ncf1": 2, "ncf2": 3}


def paper_reference(benchmark: str, method: str):
    tab = config.PAPER_TABLE_V.get(benchmark)
    if tab is None or method not in _PAPER_COL:
        return None
    return tab[_PAPER_COL[method]]        # (T, Td, Clifford)


TABLEV_FIELDS = [
    "benchmark", "method", "n_qubits", "n_paulis", "n_unitaries", "eps",
    "t_count", "t_depth", "clifford_count",
    "paper_t", "paper_td", "paper_clifford", "t_ratio_vs_paper",
]


def write_tablev_csv(rows: List[Dict[str, object]], path: str) -> None:
    out = []
    for r in rows:
        ref = paper_reference(r["benchmark"], r["method"])
        rr = dict(r)
        if ref:
            rr["paper_t"], rr["paper_td"], rr["paper_clifford"] = ref
            rr["t_ratio_vs_paper"] = f"{r['t_count'] / ref[0]:.3f}" if ref[0] else ""
        else:
            rr["paper_t"] = rr["paper_td"] = rr["paper_clifford"] = ""
            rr["t_ratio_vs_paper"] = ""
        out.append(rr)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=TABLEV_FIELDS)
        w.writeheader()
        for rr in out:
            w.writerow(rr)


def read_tablev_csv(path: str) -> List[Dict[str, object]]:
    if not os.path.isfile(path):
        return []
    with open(path, encoding="utf-8") as f:
        return list(csv.DictReader(f))


def merge_tablev_rows(existing: List[Dict[str, object]],
                      new_rows: List[Dict[str, object]]) -> List[Dict[str, object]]:
    idx = {(r["benchmark"], r["method"]): i for i, r in enumerate(existing)}
    for r in new_rows:
        key = (r["benchmark"], r["method"])
        if key in idx:
            existing[idx[key]] = r
        else:
            existing.append(r)
    return existing
