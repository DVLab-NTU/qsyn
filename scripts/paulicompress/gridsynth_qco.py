"""Gridsynth (pygridsynth) + Qsyn QCO (qzq) for Clifford+T synthesis."""

from __future__ import annotations

import math
import os
import re
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

import mpmath

from util import DEFAULT_QSYN_BIN, parse_qcir_print_stats, run_qsyn_dof

try:
    from pygridsynth import gridsynth_circuit
    from pygridsynth.quantum_gate import (
        CxGate,
        HGate,
        SGate,
        SXGate,
        TGate,
        WGate,
    )
except ImportError as exc:  # pragma: no cover
    raise ImportError("pip install pygridsynth mpmath") from exc

CLIFFORD_P_SLACK = 1e-9
_P_GATE_RE = re.compile(r"^p\s*\(([^)]+)\)\s+q\[(\d+)\]\s*;\s*$", re.IGNORECASE)
_KNOWN_2Q_RE = re.compile(
    r"^(cx|cz|swap|ecr)\s+q\[(\d+)\]\s*,\s*q\[(\d+)\]\s*;\s*$", re.IGNORECASE
)
_KNOWN_1Q_RE = re.compile(
    r"^(h|x|y|z|s|sdg|t|tdg|sx|sxdg|id|rz\s*\([^)]+\)|rx\s*\([^)]+\)|ry\s*\([^)]+\))"
    r"\s+q\[(\d+)\]\s*;\s*$",
    re.IGNORECASE,
)


def parse_qsyn_angle(expr: str) -> float:
    s = expr.strip().replace(" ", "")
    s = re.sub(r"(?<![a-zA-Z])pi", "math.pi", s)
    return float(eval(s, {"__builtins__": {}}, {"math": math}))  # noqa: S307


def _gridsynth_lines(
    theta: mpmath.mpf,
    epsilon: mpmath.mpf,
    qubit: int,
    *,
    seed: int,
    dps: int,
) -> List[str]:
    circ = gridsynth_circuit(theta, epsilon, seed=seed, dps=dps)
    circ.decompose_phase_gate()
    lines: List[str] = []
    for gate in circ:
        if isinstance(gate, WGate):
            continue
        if isinstance(gate, HGate):
            lines.append(f"h q[{qubit}];")
        elif isinstance(gate, TGate):
            lines.append(f"t q[{qubit}];")
        elif isinstance(gate, SGate):
            lines.append(f"s q[{qubit}];")
        elif isinstance(gate, SXGate):
            lines.append(f"x q[{qubit}];")
        elif isinstance(gate, CxGate):
            lines.append(
                f"cx q[{gate.control_qubit}], q[{gate.target_qubit}];"
            )
        else:
            raise RuntimeError(f"unsupported pygridsynth gate: {gate!r}")
    return lines


def gridsynth_qasm_from_qcir(
    qcir_qasm: Path,
    out_qasm: Path,
    epsilon: float,
    *,
    seed: int = 42,
    dps: int = 512,
) -> Tuple[int, int]:
    """Replace non-Clifford ``p(λ)`` with pygridsynth Clifford+T (θ = 2λ)."""
    eps_m = mpmath.mpf(epsilon)
    mpmath.mp.dps = max(dps, 64)
    src = Path(qcir_qasm).read_text(encoding="utf-8").splitlines()

    n_qubits = 0
    for line in src:
        m = re.match(r"qreg\s+q\[(\d+)\]", line.strip())
        if m:
            n_qubits = int(m.group(1))
            break
    if n_qubits <= 0:
        raise ValueError(f"no qreg in {qcir_qasm}")

    out: List[str] = [
        "OPENQASM 2.0;",
        'include "qelib1.inc";',
        f"qreg q[{n_qubits}];",
    ]
    n_synth = 0
    for raw in src:
        line = raw.strip()
        if (
            not line
            or line.startswith("//")
            or line.startswith("OPENQASM")
            or line.startswith("include")
            or line.startswith("qreg")
        ):
            continue
        pm = _P_GATE_RE.match(line)
        if pm:
            lam = parse_qsyn_angle(pm.group(1))
            qubit = int(pm.group(2))
            ratio = lam / (math.pi / 2.0) if lam != 0.0 else 0.0
            if lam == 0.0 or abs(ratio - round(ratio)) < CLIFFORD_P_SLACK:
                out.append(f"p({pm.group(1)}) q[{qubit}];")
                continue
            n_synth += 1
            theta = mpmath.mpf(2.0 * lam)
            out.extend(
                _gridsynth_lines(
                    theta, eps_m, qubit, seed=seed + n_synth, dps=dps
                )
            )
            continue
        if _KNOWN_2Q_RE.match(line) or _KNOWN_1Q_RE.match(line):
            out.append(line)
            continue
        raise ValueError(f"unsupported gate line: {line}")

    out_qasm = Path(out_qasm)
    out_qasm.parent.mkdir(parents=True, exist_ok=True)
    out_qasm.write_text("\n".join(out) + "\n", encoding="utf-8")
    t_count = sum(1 for ln in out if re.match(r"t\s+q\[", ln, re.I))
    return n_synth, t_count


def run_qzq(
    in_qasm: Path,
    out_qasm: Path,
    *,
    qsyn_bin: Path = DEFAULT_QSYN_BIN,
    timeout_s: int = 7200,
) -> Dict[str, int]:
    """Qsyn QCO alias ``qzq``: ZX full opt + qcir optimize."""
    in_q = str(Path(in_qasm).resolve()).replace('"', '\\"')
    out_q = str(Path(out_qasm).resolve()).replace('"', '\\"')
    Path(out_qasm).parent.mkdir(parents=True, exist_ok=True)
    stdout, _ = run_qsyn_dof(
        [
            f'qcir read "{in_q}"',
            "convert qcir zx",
            "zx optimize --full",
            "convert zx qcir",
            "qcir optimize",
            f'qcir write "{out_q}"',
            "qcir print",
        ],
        Path(qsyn_bin),
        timeout_s=timeout_s,
        label="qzq",
    )
    return parse_qcir_print_stats(stdout)
