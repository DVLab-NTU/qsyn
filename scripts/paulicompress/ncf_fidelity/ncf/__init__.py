"""NCF synthesis-fidelity audit (product of per-fused-unitary process F).

Public entry: ``scripts/paulicompress/cli.py ncf-fidelity``.
"""

from . import benchmarks, compile_ncf, config, fidelity, metrics, synth

__all__ = [
    "benchmarks",
    "compile_ncf",
    "config",
    "fidelity",
    "metrics",
    "synth",
]
