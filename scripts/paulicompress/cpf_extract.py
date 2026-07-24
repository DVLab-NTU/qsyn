"""Extract folded Pauli list after slow CPF (to-zyz → to-tableau --fold)."""

from __future__ import annotations

import math
import re
from pathlib import Path
from typing import List, Optional, Tuple

from pauli_list import PauliCircuit, Rotation
from util import DEFAULT_QSYN_BIN, run_qsyn_dof

_TABLEAU_SUMMARY_RE = re.compile(
    r"Tableau \((\d+) qubits, (\d+) Clifford segments, "
    r"(\d+) Pauli rotations\)"
)
_TABLEAU_ROT_RE = re.compile(r"exp\(i \* (.+?) \* ([XYZI+\-]+)\)", re.IGNORECASE)
TWO_PI = 2.0 * math.pi


def _parse_qsyn_phase_expr(expr: str) -> float:
    s = expr.strip().replace("π", "pi")
    s = re.sub(r"(\d)pi", r"\1*pi", s)
    if s.startswith("pi"):
        s = "1*" + s
    elif s.startswith("-pi"):
        s = "-1*" + s[1:]
    allowed = set("0123456789./*+-e()")
    s_no_pi = s.replace("pi", "")
    if not all(c in allowed for c in s_no_pi):
        raise ValueError(f"unsafe phase expression: {expr!r}")
    return float(eval(s, {"__builtins__": {}}, {"pi": math.pi}))


def nearest_unwrap(wrapped: float, reference: float) -> float:
    best = wrapped
    best_d = abs(wrapped - reference)
    k_lo = int(math.floor((reference - wrapped - math.pi) / TWO_PI)) - 1
    k_hi = int(math.ceil((reference - wrapped + math.pi) / TWO_PI)) + 1
    for k in range(k_lo, k_hi + 1):
        cand = wrapped + k * TWO_PI
        d = abs(cand - reference)
        if d < best_d - 1e-15:
            best_d = d
            best = cand
    return best


def parse_tableau_char_output(
    text: str,
    *,
    raw_angles: Optional[dict] = None,
    reverse_qubits: bool = False,
) -> Tuple[List[Rotation], dict]:
    summary = _TABLEAU_SUMMARY_RE.search(text)
    meta = {
        "n_qubits": int(summary.group(1)) if summary else 0,
        "clifford_segments": int(summary.group(2)) if summary else 0,
        "tableau_pauli_rotations": int(summary.group(3)) if summary else 0,
    }
    rots: List[Rotation] = []
    in_rot = False
    for line in text.splitlines():
        if line.strip() == "Pauli Rotations:":
            in_rot = True
            continue
        if not in_rot:
            continue
        m = _TABLEAU_ROT_RE.search(line)
        if not m:
            continue
        phase = _parse_qsyn_phase_expr(m.group(1))
        pauli = m.group(2).lstrip("+")
        if reverse_qubits:
            pauli = pauli[::-1]
        if raw_angles is not None and pauli in raw_angles:
            phase = nearest_unwrap(phase, raw_angles[pauli])
        rots.append(Rotation(pauli, phase))
    meta["parsed_pauli_rotations"] = len(rots)
    return rots, meta


def extract_after_slow_cpf(
    qasm_path: Path,
    *,
    name: str,
    qsyn_bin: Path = DEFAULT_QSYN_BIN,
    raw_pauli: Optional[PauliCircuit] = None,
    timeout_s: int = 14400,
) -> PauliCircuit:
    """Run slow CPF and return the folded Pauli list (MSB-first convention)."""
    iq = str(Path(qasm_path).resolve()).replace('"', '\\"')
    stdout, _ = run_qsyn_dof(
        [
            f'qcir read "{iq}"',
            "qcir to-zyz -r",
            "qcir to-tableau --fold -r",
            "tableau print",
            "tableau print -c",
        ],
        Path(qsyn_bin),
        timeout_s=timeout_s,
        label="slow/cpf-extract",
    )
    raw_angles = None
    if raw_pauli is not None:
        raw_angles = {r.pauli: r.theta for r in raw_pauli.rotations}
    # slow path OpenQASM round-trip reverses qubit order in print -c
    rots, meta = parse_tableau_char_output(
        stdout, raw_angles=raw_angles, reverse_qubits=True
    )
    nq = meta["n_qubits"] or (len(rots[0].pauli) if rots else 0)
    if nq <= 0:
        raise RuntimeError(f"failed to parse folded Pauli list from {qasm_path}")
    return PauliCircuit(name, nq, rots)
