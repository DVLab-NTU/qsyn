"""Slow / fast Proposed Synthesis Flow runners (thesis Chapters 3–5 core).

Default compression is the thesis zero-sweep F-cost heuristic
(``zero_sweep.plan_min_pauli_rotation``). Optional ``cpp-l2`` uses Qsyn
``tableau optimize pauli-compress -l``.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, Optional

from cpf_extract import extract_after_slow_cpf
from gridsynth_qco import gridsynth_qasm_from_qcir, run_qzq
from pauli_list import load_pauli_file, write_pauli_file
from util import DEFAULT_QSYN_BIN, run_qsyn_dof
from zero_sweep import fidelity_to_l2_budget, plan_min_pauli_rotation


@dataclass
class FlowResult:
    mode: str
    bench: str
    fidelity: float
    compress: str
    l2_budget: float
    epsilon: float
    input_pauli: str
    input_qasm: str
    folded_pauli: str
    compressed_pauli: str
    compressed_qcir: str
    gridsynth_qasm: str
    final_qasm: str
    n_rot_before: int
    n_rot_after: int
    fidelity_exact: float
    n_p_synthesized: int
    gridsynth_t_count: int
    qzq_gate_count: int
    qzq_t_count: int
    extra: Dict[str, Any] = field(default_factory=dict)


def _resolve_inputs(bench_dir: Path, bench: str) -> tuple[Path, Path]:
    bench_dir = Path(bench_dir)
    for d in (bench_dir / bench, bench_dir):
        pauli = d / f"{bench}.pauli"
        qasm = d / f"{bench}.qasm"
        if pauli.is_file():
            return pauli, qasm if qasm.is_file() else pauli
    raise FileNotFoundError(
        f"cannot find {bench}.pauli under {bench_dir} "
        f"(expected {bench_dir / bench / (bench + '.pauli')})"
    )


def _pauli_to_qcir(
    pauli_path: Path,
    out_qasm: Path,
    *,
    qsyn_bin: Path,
) -> None:
    pp = str(Path(pauli_path).resolve()).replace('"', '\\"')
    qq = str(Path(out_qasm).resolve()).replace('"', '\\"')
    Path(out_qasm).parent.mkdir(parents=True, exist_ok=True)
    run_qsyn_dof(
        [
            f'tableau read-pauli "{pp}"',
            "tableau optimize collapse",
            "convert tableau qcir",
            f'qcir write "{qq}"',
            "qcir print",
        ],
        Path(qsyn_bin),
        label="pauli→qcir",
    )


def _finish_ct(
    *,
    qcir: Path,
    gs: Path,
    final: Path,
    epsilon: float,
    seed: int,
    qsyn_bin: Path,
    skip_qzq: bool,
) -> tuple[int, int, dict]:
    n_synth, t_gs = gridsynth_qasm_from_qcir(qcir, gs, epsilon, seed=seed)
    qzq_stats: dict = {}
    if skip_qzq:
        final.write_text(gs.read_text(encoding="utf-8"), encoding="utf-8")
    else:
        qzq_stats = run_qzq(gs, final, qsyn_bin=Path(qsyn_bin))
    return n_synth, t_gs, qzq_stats


def run_fast_zero_sweep(
    *,
    pauli_path: Path,
    out_dir: Path,
    fidelity: float,
    epsilon: float,
    qsyn_bin: Path = DEFAULT_QSYN_BIN,
    seed: int = 42,
    skip_qzq: bool = False,
) -> FlowResult:
    """fast: Hamiltonian .pauli → zero-sweep → Gridsynth → qzq."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    pauli_path = Path(pauli_path).resolve()
    pc = load_pauli_file(pauli_path)
    plan = plan_min_pauli_rotation(pc, fidelity)

    folded = out_dir / "input.pauli"
    compressed = out_dir / "after_zero_sweep.pauli"
    write_pauli_file(pc, folded)
    write_pauli_file(plan.compressed, compressed)

    qcir = out_dir / "after_pauli_compress.qasm"
    gs = out_dir / "after_gridsynth.qasm"
    final = out_dir / "clifford_t.qasm"
    _pauli_to_qcir(compressed, qcir, qsyn_bin=qsyn_bin)
    n_synth, t_gs, qzq_stats = _finish_ct(
        qcir=qcir,
        gs=gs,
        final=final,
        epsilon=epsilon,
        seed=seed,
        qsyn_bin=qsyn_bin,
        skip_qzq=skip_qzq,
    )
    return FlowResult(
        mode="fast",
        bench=pauli_path.stem,
        fidelity=fidelity,
        compress="zero-sweep",
        l2_budget=plan.l2_budget_seed,
        epsilon=epsilon,
        input_pauli=str(pauli_path),
        input_qasm="",
        folded_pauli=str(folded),
        compressed_pauli=str(compressed),
        compressed_qcir=str(qcir),
        gridsynth_qasm=str(gs),
        final_qasm=str(final),
        n_rot_before=plan.n_rot_before,
        n_rot_after=plan.n_rot_after,
        fidelity_exact=plan.fidelity_exact,
        n_p_synthesized=n_synth,
        gridsynth_t_count=t_gs,
        qzq_gate_count=int(qzq_stats.get("n_gates", 0)),
        qzq_t_count=int(qzq_stats.get("n_t_gates", 0)),
        extra=plan.summary_dict(),
    )


def run_slow_zero_sweep(
    *,
    qasm_path: Path,
    pauli_path: Optional[Path],
    out_dir: Path,
    fidelity: float,
    epsilon: float,
    qsyn_bin: Path = DEFAULT_QSYN_BIN,
    seed: int = 42,
    skip_qzq: bool = False,
) -> FlowResult:
    """slow: QASM → CPF (to-zyz + fold/PauliDAG) → zero-sweep → Gridsynth → qzq."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    qasm_path = Path(qasm_path).resolve()
    raw = load_pauli_file(pauli_path) if pauli_path and Path(pauli_path).is_file() else None
    folded_pc = extract_after_slow_cpf(
        qasm_path,
        name=qasm_path.stem,
        qsyn_bin=qsyn_bin,
        raw_pauli=raw,
    )
    plan = plan_min_pauli_rotation(folded_pc, fidelity)

    folded = out_dir / "after_cpf.pauli"
    compressed = out_dir / "after_zero_sweep.pauli"
    write_pauli_file(folded_pc, folded)
    write_pauli_file(plan.compressed, compressed)

    qcir = out_dir / "after_pauli_compress.qasm"
    gs = out_dir / "after_gridsynth.qasm"
    final = out_dir / "clifford_t.qasm"
    _pauli_to_qcir(compressed, qcir, qsyn_bin=qsyn_bin)
    n_synth, t_gs, qzq_stats = _finish_ct(
        qcir=qcir,
        gs=gs,
        final=final,
        epsilon=epsilon,
        seed=seed,
        qsyn_bin=qsyn_bin,
        skip_qzq=skip_qzq,
    )
    return FlowResult(
        mode="slow",
        bench=qasm_path.stem,
        fidelity=fidelity,
        compress="zero-sweep",
        l2_budget=plan.l2_budget_seed,
        epsilon=epsilon,
        input_pauli=str(pauli_path) if pauli_path else "",
        input_qasm=str(qasm_path),
        folded_pauli=str(folded),
        compressed_pauli=str(compressed),
        compressed_qcir=str(qcir),
        gridsynth_qasm=str(gs),
        final_qasm=str(final),
        n_rot_before=plan.n_rot_before,
        n_rot_after=plan.n_rot_after,
        fidelity_exact=plan.fidelity_exact,
        n_p_synthesized=n_synth,
        gridsynth_t_count=t_gs,
        qzq_gate_count=int(qzq_stats.get("n_gates", 0)),
        qzq_t_count=int(qzq_stats.get("n_t_gates", 0)),
        extra=plan.summary_dict(),
    )


def run_fast_cpp_l2(
    *,
    pauli_path: Path,
    out_dir: Path,
    l2_budget: float,
    epsilon: float,
    qsyn_bin: Path = DEFAULT_QSYN_BIN,
    seed: int = 42,
    skip_qzq: bool = False,
) -> FlowResult:
    """Optional: C++ ``pauli-compress -l`` (L2 surrogate, not thesis F-cost)."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    pauli_path = Path(pauli_path).resolve()
    qcir = out_dir / "after_pauli_compress.qasm"
    gs = out_dir / "after_gridsynth.qasm"
    final = out_dir / "clifford_t.qasm"
    pp = str(pauli_path).replace('"', '\\"')
    qq = str(qcir.resolve()).replace('"', '\\"')
    run_qsyn_dof(
        [
            f'tableau read-pauli "{pp}"',
            f"tableau optimize pauli-compress -l {l2_budget}",
            "convert tableau qcir",
            f'qcir write "{qq}"',
            "qcir print",
        ],
        Path(qsyn_bin),
        label="fast/cpp-l2",
    )
    n_synth, t_gs, qzq_stats = _finish_ct(
        qcir=qcir, gs=gs, final=final, epsilon=epsilon, seed=seed,
        qsyn_bin=qsyn_bin, skip_qzq=skip_qzq,
    )
    return FlowResult(
        mode="fast",
        bench=pauli_path.stem,
        fidelity=0.0,
        compress="cpp-l2",
        l2_budget=l2_budget,
        epsilon=epsilon,
        input_pauli=str(pauli_path),
        input_qasm="",
        folded_pauli="",
        compressed_pauli="",
        compressed_qcir=str(qcir),
        gridsynth_qasm=str(gs),
        final_qasm=str(final),
        n_rot_before=0,
        n_rot_after=0,
        fidelity_exact=0.0,
        n_p_synthesized=n_synth,
        gridsynth_t_count=t_gs,
        qzq_gate_count=int(qzq_stats.get("n_gates", 0)),
        qzq_t_count=int(qzq_stats.get("n_t_gates", 0)),
    )


def run_proposed_flow(
    *,
    mode: str,
    bench: str,
    bench_root: Path,
    out_root: Path,
    fidelity: float = 0.99,
    epsilon: float = 1e-3,
    qsyn_bin: Path = DEFAULT_QSYN_BIN,
    seed: int = 42,
    skip_qzq: bool = False,
    compress: str = "zero-sweep",
    l2_budget: Optional[float] = None,
) -> FlowResult:
    if mode not in {"fast", "slow"}:
        raise ValueError(f"mode must be fast|slow, got {mode!r}")
    if compress not in {"zero-sweep", "cpp-l2"}:
        raise ValueError(f"compress must be zero-sweep|cpp-l2, got {compress!r}")

    pauli_path, qasm_path = _resolve_inputs(Path(bench_root), bench)
    out_dir = Path(out_root) / mode / bench
    budget = fidelity_to_l2_budget(fidelity) if l2_budget is None else float(l2_budget)

    if compress == "cpp-l2":
        if mode != "fast":
            raise ValueError("cpp-l2 compress currently wired for --mode fast only")
        result = run_fast_cpp_l2(
            pauli_path=pauli_path,
            out_dir=out_dir,
            l2_budget=budget,
            epsilon=epsilon,
            qsyn_bin=qsyn_bin,
            seed=seed,
            skip_qzq=skip_qzq,
        )
        result.fidelity = fidelity
    elif mode == "fast":
        result = run_fast_zero_sweep(
            pauli_path=pauli_path,
            out_dir=out_dir,
            fidelity=fidelity,
            epsilon=epsilon,
            qsyn_bin=qsyn_bin,
            seed=seed,
            skip_qzq=skip_qzq,
        )
    else:
        qasm = qasm_path if qasm_path.suffix == ".qasm" else pauli_path.with_suffix(".qasm")
        result = run_slow_zero_sweep(
            qasm_path=qasm,
            pauli_path=pauli_path,
            out_dir=out_dir,
            fidelity=fidelity,
            epsilon=epsilon,
            qsyn_bin=qsyn_bin,
            seed=seed,
            skip_qzq=skip_qzq,
        )

    result.bench = bench
    summary = out_dir / "summary.json"
    payload = asdict(result)
    summary.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    print(
        f"[{mode}/{result.compress}] {bench}: F*={fidelity} "
        f"#rot {result.n_rot_before}->{result.n_rot_after} "
        f"(F≈{result.fidelity_exact:.6f}) "
        f"gridsynth_T={result.gridsynth_t_count} qzq_T={result.qzq_t_count} "
        f"→ {result.final_qasm}"
    )
    return result
