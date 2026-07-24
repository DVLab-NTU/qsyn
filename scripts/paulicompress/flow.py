"""Proposed Synthesis Flow runners with optional steps.

Users who already know the stages can compose::

    hamiltonian
      [→ Phase Folding]
      [→ Pauli Compression: zero-sweep | methods-ae | cpp-l2 | none]
      [→ Gridsynth]
      [→ qzq]

``run`` / ``all`` keep the classic fast/slow wrappers; ``pipeline`` exposes
the knobs explicitly.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, Optional

from cpf_extract import extract_after_slow_cpf
from experiment_best import plan_methods_ae
from gridsynth_qco import gridsynth_qasm_from_qcir, run_qzq
from methods_ae import count_nonclifford
from pauli_list import PauliCircuit, load_pauli_file, write_pauli_file
from util import DEFAULT_QSYN_BIN, run_qsyn_dof
from zero_sweep import fidelity_to_l2_budget, plan_min_pauli_rotation

COMPRESS_CHOICES = ("none", "zero-sweep", "methods-ae", "cpp-l2")


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
    phase_fold: bool = False
    do_gridsynth: bool = True
    do_qzq: bool = True
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
    skip_gridsynth: bool,
    skip_qzq: bool,
) -> tuple[int, int, dict]:
    if skip_gridsynth:
        # Pass compressed qcir through as the "final" artifact.
        text = Path(qcir).read_text(encoding="utf-8")
        gs.write_text(text, encoding="utf-8")
        final.write_text(text, encoding="utf-8")
        return 0, 0, {}
    n_synth, t_gs = gridsynth_qasm_from_qcir(qcir, gs, epsilon, seed=seed)
    qzq_stats: dict = {}
    if skip_qzq:
        final.write_text(gs.read_text(encoding="utf-8"), encoding="utf-8")
    else:
        qzq_stats = run_qzq(gs, final, qsyn_bin=Path(qsyn_bin))
    return n_synth, t_gs, qzq_stats


def _compress_pauli(
    pc: PauliCircuit,
    *,
    compress: str,
    fidelity: float,
    l2_budget: Optional[float],
    qsyn_bin: Path,
    work_dir: Path,
) -> tuple[PauliCircuit, int, int, float, float, dict]:
    """Return (compressed, n_before, n_after, F_exact, l2_used, extra)."""
    compress = compress.lower()
    n_before = count_nonclifford(pc)
    if compress in {"", "none"}:
        return pc.copy(), n_before, n_before, 1.0, 0.0, {"compress": "none"}

    if compress == "zero-sweep":
        plan = plan_min_pauli_rotation(pc, fidelity)
        return (
            plan.compressed,
            plan.n_rot_before,
            plan.n_rot_after,
            plan.fidelity_exact,
            plan.l2_used,
            plan.summary_dict(),
        )

    if compress in {"methods-ae", "methods_ae", "ae"}:
        plan = plan_methods_ae(pc, fidelity)
        return (
            plan.compressed,
            plan.n_rot_before,
            plan.n_rot_after,
            plan.approx_fidelity,
            plan.l2_err,
            plan.summary_dict(),
        )

    if compress == "cpp-l2":
        bud = (
            float(l2_budget)
            if l2_budget is not None
            else fidelity_to_l2_budget(fidelity)
        )
        work_dir.mkdir(parents=True, exist_ok=True)
        inp = work_dir / "_cpp_l2_in.pauli"
        out_q = work_dir / "_cpp_l2_out.qasm"
        write_pauli_file(pc, inp)
        pp = str(inp.resolve()).replace('"', '\\"')
        qq = str(out_q.resolve()).replace('"', '\\"')
        run_qsyn_dof(
            [
                f'tableau read-pauli "{pp}"',
                f"tableau optimize pauli-compress -l {bud}",
                "convert tableau qcir",
                f'qcir write "{qq}"',
                "qcir print",
            ],
            Path(qsyn_bin),
            label="cpp-l2",
        )
        # cpp-l2 already produced qcir; keep Pauli identity for bookkeeping
        return pc.copy(), n_before, n_before, 0.0, bud, {"compress": "cpp-l2", "l2": bud, "qcir": str(out_q)}

    raise ValueError(
        f"unknown compress {compress!r}; expected one of {COMPRESS_CHOICES}"
    )


def run_pipeline(
    *,
    out_dir: Path,
    pauli_path: Optional[Path] = None,
    qasm_path: Optional[Path] = None,
    phase_fold: bool = False,
    compress: str = "zero-sweep",
    fidelity: float = 0.99,
    l2_budget: Optional[float] = None,
    gridsynth: bool = True,
    qzq: bool = True,
    epsilon: float = 1e-3,
    qsyn_bin: Path = DEFAULT_QSYN_BIN,
    seed: int = 42,
    name: Optional[str] = None,
) -> FlowResult:
    """Composable Proposed Flow.

    Parameters
    ----------
    phase_fold:
        If True, run slow CPF (needs ``qasm_path``).
    compress:
        ``none`` | ``zero-sweep`` | ``methods-ae`` | ``cpp-l2``.
    gridsynth / qzq:
        Optional Clifford+T backend steps (``qzq`` implies Gridsynth unless
        both are skipped).
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    compress = compress.lower().replace("_", "-")
    if compress == "ae":
        compress = "methods-ae"

    if pauli_path is None and qasm_path is None:
        raise ValueError("need --pauli and/or --qasm")

    raw_pc: Optional[PauliCircuit] = None
    if pauli_path is not None and Path(pauli_path).is_file():
        raw_pc = load_pauli_file(pauli_path, name=name)

    if phase_fold:
        if qasm_path is None or not Path(qasm_path).is_file():
            raise ValueError("phase_fold=True requires an existing --qasm file")
        working = extract_after_slow_cpf(
            Path(qasm_path),
            name=(raw_pc.name if raw_pc else Path(qasm_path).stem),
            qsyn_bin=Path(qsyn_bin),
            raw_pauli=raw_pc,
        )
        folded_path = out_dir / "after_phase_fold.pauli"
        write_pauli_file(working, folded_path)
    else:
        if raw_pc is None:
            raise ValueError("without phase_fold, --pauli is required")
        working = raw_pc
        folded_path = out_dir / "hamiltonian.pauli"
        write_pauli_file(working, folded_path)

    n_before = count_nonclifford(working)
    extra: Dict[str, Any] = {}
    l2_used = 0.0
    fid_exact = 1.0
    n_after = n_before
    compressed_path = out_dir / "after_compress.pauli"
    qcir = out_dir / "after_pauli_compress.qasm"

    if compress == "cpp-l2":
        _, n_before, n_after, fid_exact, l2_used, extra = _compress_pauli(
            working,
            compress=compress,
            fidelity=fidelity,
            l2_budget=l2_budget,
            qsyn_bin=Path(qsyn_bin),
            work_dir=out_dir,
        )
        # cpp-l2 wrote qcir directly
        src_q = Path(extra["qcir"])
        qcir.write_text(src_q.read_text(encoding="utf-8"), encoding="utf-8")
        write_pauli_file(working, compressed_path)  # pre-cpp snapshot
        compressed_pc = working
    else:
        compressed_pc, n_before, n_after, fid_exact, l2_used, extra = _compress_pauli(
            working,
            compress=compress,
            fidelity=fidelity,
            l2_budget=l2_budget,
            qsyn_bin=Path(qsyn_bin),
            work_dir=out_dir,
        )
        write_pauli_file(compressed_pc, compressed_path)
        if gridsynth or qzq:
            _pauli_to_qcir(compressed_path, qcir, qsyn_bin=Path(qsyn_bin))
        else:
            # still emit a qcir for inspection when CT is skipped
            _pauli_to_qcir(compressed_path, qcir, qsyn_bin=Path(qsyn_bin))

    gs = out_dir / "after_gridsynth.qasm"
    final = out_dir / "clifford_t.qasm"
    skip_gs = not gridsynth
    skip_qzq = (not qzq) or skip_gs
    n_synth, t_gs, qzq_stats = _finish_ct(
        qcir=qcir,
        gs=gs,
        final=final,
        epsilon=epsilon,
        seed=seed,
        qsyn_bin=Path(qsyn_bin),
        skip_gridsynth=skip_gs,
        skip_qzq=skip_qzq,
    )

    mode = "slow" if phase_fold else "fast"
    result = FlowResult(
        mode=mode,
        bench=name or (raw_pc.name if raw_pc else Path(folded_path).stem),
        fidelity=fidelity,
        compress=compress,
        l2_budget=float(l2_used),
        epsilon=epsilon,
        input_pauli=str(pauli_path) if pauli_path else "",
        input_qasm=str(qasm_path) if qasm_path else "",
        folded_pauli=str(folded_path),
        compressed_pauli=str(compressed_path),
        compressed_qcir=str(qcir),
        gridsynth_qasm=str(gs),
        final_qasm=str(final),
        n_rot_before=n_before,
        n_rot_after=n_after,
        fidelity_exact=float(fid_exact),
        n_p_synthesized=n_synth,
        gridsynth_t_count=t_gs,
        qzq_gate_count=int(qzq_stats.get("n_gates", 0)),
        qzq_t_count=int(qzq_stats.get("n_t_gates", 0)),
        phase_fold=phase_fold,
        do_gridsynth=gridsynth,
        do_qzq=not skip_qzq,
        extra=extra,
    )
    (out_dir / "summary.json").write_text(
        json.dumps(asdict(result), indent=2), encoding="utf-8"
    )
    print(
        f"[pipeline/{mode}/{compress}] {result.bench}: F*={fidelity} "
        f"fold={'on' if phase_fold else 'off'} "
        f"gs={'on' if gridsynth else 'off'} qzq={'on' if result.do_qzq else 'off'} "
        f"#rot {n_before}->{n_after} (F≈{fid_exact:.6f}) "
        f"→ {result.final_qasm}"
    )
    return result


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
    return run_pipeline(
        out_dir=out_dir,
        pauli_path=pauli_path,
        phase_fold=False,
        compress="zero-sweep",
        fidelity=fidelity,
        gridsynth=True,
        qzq=not skip_qzq,
        epsilon=epsilon,
        qsyn_bin=qsyn_bin,
        seed=seed,
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
    return run_pipeline(
        out_dir=out_dir,
        pauli_path=pauli_path,
        qasm_path=qasm_path,
        phase_fold=True,
        compress="zero-sweep",
        fidelity=fidelity,
        gridsynth=True,
        qzq=not skip_qzq,
        epsilon=epsilon,
        qsyn_bin=qsyn_bin,
        seed=seed,
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
    return run_pipeline(
        out_dir=out_dir,
        pauli_path=pauli_path,
        phase_fold=False,
        compress="cpp-l2",
        fidelity=1.0,
        l2_budget=l2_budget,
        gridsynth=True,
        qzq=not skip_qzq,
        epsilon=epsilon,
        qsyn_bin=qsyn_bin,
        seed=seed,
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
    gridsynth: bool = True,
) -> FlowResult:
    if mode not in {"fast", "slow"}:
        raise ValueError(f"mode must be fast|slow, got {mode!r}")
    compress = compress.lower().replace("_", "-")
    if compress not in COMPRESS_CHOICES:
        raise ValueError(f"compress must be one of {COMPRESS_CHOICES}, got {compress!r}")

    pauli_path, qasm_path = _resolve_inputs(Path(bench_root), bench)
    out_dir = Path(out_root) / mode / bench
    qasm = qasm_path if qasm_path.suffix == ".qasm" else pauli_path.with_suffix(".qasm")

    result = run_pipeline(
        out_dir=out_dir,
        pauli_path=pauli_path,
        qasm_path=qasm if mode == "slow" else None,
        phase_fold=(mode == "slow"),
        compress=compress,
        fidelity=fidelity,
        l2_budget=l2_budget,
        gridsynth=gridsynth,
        qzq=(gridsynth and not skip_qzq),
        epsilon=epsilon,
        qsyn_bin=qsyn_bin,
        seed=seed,
        name=bench,
    )
    result.bench = bench
    return result
