"""Slow / fast Proposed Synthesis Flow runners."""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Optional

from gridsynth_qco import gridsynth_qasm_from_qcir, run_qzq
from util import DEFAULT_QSYN_BIN, fidelity_to_l2_budget, parse_qcir_print_stats, run_qsyn_dof


@dataclass
class FlowResult:
    mode: str
    bench: str
    fidelity: float
    l2_budget: float
    epsilon: float
    input_pauli: str
    input_qasm: str
    compressed_qcir: str
    gridsynth_qasm: str
    final_qasm: str
    n_p_synthesized: int
    gridsynth_t_count: int
    qzq_gate_count: int
    qzq_t_count: int


def _resolve_inputs(bench_dir: Path, bench: str) -> tuple[Path, Path]:
    """Accept ``.../LiH`` or ``.../01_original_benchmarks/LiH`` layouts."""
    bench_dir = Path(bench_dir)
    candidates = [
        bench_dir / bench,
        bench_dir,
    ]
    for d in candidates:
        pauli = d / f"{bench}.pauli"
        qasm = d / f"{bench}.qasm"
        if pauli.is_file():
            return pauli, qasm if qasm.is_file() else pauli
    raise FileNotFoundError(
        f"cannot find {bench}.pauli under {bench_dir} "
        f"(expected {bench_dir / bench / (bench + '.pauli')})"
    )


def run_fast(
    *,
    pauli_path: Path,
    out_dir: Path,
    l2_budget: float,
    epsilon: float,
    qsyn_bin: Path = DEFAULT_QSYN_BIN,
    seed: int = 42,
    skip_qzq: bool = False,
) -> FlowResult:
    """fast: Hamiltonian Pauli list → PauliCompress → Gridsynth → qzq."""
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
        label="fast/pauli-compress",
    )

    n_synth, t_gs = gridsynth_qasm_from_qcir(qcir, gs, epsilon, seed=seed)
    qzq_stats: dict = {}
    if skip_qzq:
        final.write_text(gs.read_text(encoding="utf-8"), encoding="utf-8")
    else:
        qzq_stats = run_qzq(gs, final, qsyn_bin=Path(qsyn_bin))

    return FlowResult(
        mode="fast",
        bench=pauli_path.stem,
        fidelity=0.0,
        l2_budget=l2_budget,
        epsilon=epsilon,
        input_pauli=str(pauli_path),
        input_qasm="",
        compressed_qcir=str(qcir),
        gridsynth_qasm=str(gs),
        final_qasm=str(final),
        n_p_synthesized=n_synth,
        gridsynth_t_count=t_gs,
        qzq_gate_count=int(qzq_stats.get("n_gates", 0)),
        qzq_t_count=int(qzq_stats.get("n_t_gates", 0)),
    )


def run_slow(
    *,
    qasm_path: Path,
    out_dir: Path,
    l2_budget: float,
    epsilon: float,
    qsyn_bin: Path = DEFAULT_QSYN_BIN,
    seed: int = 42,
    skip_qzq: bool = False,
) -> FlowResult:
    """slow: QASM → to-zyz → to-tableau --fold (PauliDAG) → PauliCompress → Gridsynth → qzq.

    ``qcir to-tableau --fold`` = trace_replay → dag_fold → collapse (+ lossless merge).
    Lossy heuristic compression is then applied with ``pauli-compress -l``.
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    qasm_path = Path(qasm_path).resolve()
    if not qasm_path.is_file():
        raise FileNotFoundError(
            f"slow mode needs OpenQASM input, missing: {qasm_path}"
        )

    qcir = out_dir / "after_pauli_compress.qasm"
    gs = out_dir / "after_gridsynth.qasm"
    final = out_dir / "clifford_t.qasm"

    iq = str(qasm_path).replace('"', '\\"')
    qq = str(qcir.resolve()).replace('"', '\\"')
    run_qsyn_dof(
        [
            f'qcir read "{iq}"',
            "qcir to-zyz -r",
            "qcir to-tableau --fold -r",
            f"tableau optimize pauli-compress -l {l2_budget}",
            "convert tableau qcir",
            f'qcir write "{qq}"',
            "qcir print",
        ],
        Path(qsyn_bin),
        label="slow/cpf+pauli-compress",
        timeout_s=14400,
    )

    n_synth, t_gs = gridsynth_qasm_from_qcir(qcir, gs, epsilon, seed=seed)
    qzq_stats: dict = {}
    if skip_qzq:
        final.write_text(gs.read_text(encoding="utf-8"), encoding="utf-8")
    else:
        qzq_stats = run_qzq(gs, final, qsyn_bin=Path(qsyn_bin))

    return FlowResult(
        mode="slow",
        bench=qasm_path.stem,
        fidelity=0.0,
        l2_budget=l2_budget,
        epsilon=epsilon,
        input_pauli="",
        input_qasm=str(qasm_path),
        compressed_qcir=str(qcir),
        gridsynth_qasm=str(gs),
        final_qasm=str(final),
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
    l2_budget: Optional[float] = None,
) -> FlowResult:
    """Run ``fast`` or ``slow`` Proposed Synthesis Flow for one benchmark."""
    if mode not in {"fast", "slow"}:
        raise ValueError(f"mode must be fast|slow, got {mode!r}")

    pauli_path, qasm_path = _resolve_inputs(Path(bench_root), bench)
    budget = fidelity_to_l2_budget(fidelity) if l2_budget is None else float(l2_budget)
    out_dir = Path(out_root) / mode / bench

    if mode == "fast":
        result = run_fast(
            pauli_path=pauli_path,
            out_dir=out_dir,
            l2_budget=budget,
            epsilon=epsilon,
            qsyn_bin=qsyn_bin,
            seed=seed,
            skip_qzq=skip_qzq,
        )
    else:
        result = run_slow(
            qasm_path=qasm_path if qasm_path.suffix == ".qasm" else pauli_path.with_suffix(".qasm"),
            out_dir=out_dir,
            l2_budget=budget,
            epsilon=epsilon,
            qsyn_bin=qsyn_bin,
            seed=seed,
            skip_qzq=skip_qzq,
        )

    result.fidelity = fidelity
    result.bench = bench
    summary = out_dir / "summary.json"
    summary.write_text(json.dumps(asdict(result), indent=2), encoding="utf-8")
    print(
        f"[{mode}] {bench}: L2={budget:.6g} eps={epsilon:g} "
        f"gridsynth_T={result.gridsynth_t_count} qzq_T={result.qzq_t_count} "
        f"→ {result.final_qasm}"
    )
    return result
