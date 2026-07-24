#!/usr/bin/env python3
"""Synthesize one logical block to U3+CX using selected BQSKit passes.

Usage:
    python3 scripts/bqskit_block_synth.py --engine qsearch --opt 1 IN.qasm OUT.qasm

Engines: compile, qsearch, leap, qfast, qpredict
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def _workflow(engine: str, block_size: int, opt_level: int):
    from bqskit.compiler import Compiler
    from bqskit.passes import (
        ForEachBlockPass,
        QuickPartitioner,
        ScanningGateRemovalPass,
        ToU3Pass,
        UnfoldPass,
    )

    instantiate = {"cost_fn": "hilbert-schmidt", "method": "qfactor"}

    if engine == "compile":
        from bqskit import compile as bqskit_compile

        def run(circuit):
            return bqskit_compile(circuit, optimization_level=opt_level)

        return run

    synth_pass = None
    if engine == "qsearch":
        from bqskit.passes import QSearchSynthesisPass

        synth_pass = QSearchSynthesisPass(instantiate_options=instantiate)
    elif engine == "leap":
        from bqskit.passes import LEAPSynthesisPass

        synth_pass = LEAPSynthesisPass(instantiate_options=instantiate)
    elif engine == "qfast":
        from bqskit.passes.synthesis.qfast import QFASTDecompositionPass

        synth_pass = QFASTDecompositionPass(instantiate_options=instantiate)
    elif engine == "qpredict":
        from bqskit.passes.synthesis.qpredict import QPredictDecompositionPass

        synth_pass = QPredictDecompositionPass(instantiate_options=instantiate)
    else:
        raise ValueError(f"unknown engine: {engine}")

    block_workflow = [
        ScanningGateRemovalPass(instantiate_options=instantiate),
        synth_pass,
        ScanningGateRemovalPass(instantiate_options=instantiate),
    ]
    workflow = [
        QuickPartitioner(block_size),
        ForEachBlockPass(block_workflow),
        UnfoldPass(),
        ToU3Pass(),
    ]
    compiler = Compiler()
    return lambda circuit: compiler.compile(circuit, workflow)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--engine",
        choices=("compile", "qsearch", "leap", "qfast", "qpredict"),
        default="qsearch",
    )
    parser.add_argument("--opt", type=int, default=1, help="optimization_level for compile engine")
    parser.add_argument("--block-size", type=int, default=3)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    try:
        from bqskit.ir import Circuit
    except ImportError as exc:
        print("bqskit_block_synth.py requires bqskit: pip install bqskit", file=sys.stderr)
        raise SystemExit(1) from exc

    circuit = Circuit.from_file(str(args.input))
    run = _workflow(args.engine, args.block_size, args.opt)
    out = run(circuit)
    out.save(str(args.output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
