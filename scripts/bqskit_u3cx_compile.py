#!/usr/bin/env python3
"""Compile an OpenQASM file to U3+CNOT using BQSKit.

Usage:
    python3 scripts/bqskit_u3cx_compile.py [--opt LEVEL] INPUT.qasm OUTPUT.qasm

Requires: pip install bqskit
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--opt", type=int, default=1, help="BQSKit optimization_level (default 1)")
    parser.add_argument("input", type=Path, help="input QASM path")
    parser.add_argument("output", type=Path, help="output QASM path")
    args = parser.parse_args()

    try:
        from bqskit import compile as bqskit_compile
        from bqskit.ir import Circuit
    except ImportError as exc:
        print("bqskit_u3cx_compile.py requires bqskit: pip install bqskit", file=sys.stderr)
        raise SystemExit(1) from exc

    circuit = Circuit.from_file(str(args.input))
    out = bqskit_compile(circuit, optimization_level=args.opt)
    out.save(str(args.output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
