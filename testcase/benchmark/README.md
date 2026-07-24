# `testcase/benchmark/` — PR-11 micro-benchmark suite

A small collection of QASM files used by `scripts/cpf_bench.py` to drive
the CPF pipeline end-to-end and emit the Markdown report at
`docs/benchmark_results.md`.

## Files

| QASM | Purpose |
| --- | --- |
| `qasm/bench_2q_easy.qasm`       | Trivially mergeable: two `RZ` plus two `RX` on independent qubits. Sanity check that the trace-replay / rebuild round-trip preserves shape. |
| `qasm/bench_2q_clifford.qasm`   | Two `RZ` on the control of a `CX` -- exercises `cpf-merge` (propagation through a single Clifford). |
| `qasm/bench_2q_continuous.qasm` | `RZ(pi/3) - CX - RZ(2pi/5) - CX` -- continuous angles that TODD-only `phasepoly` cannot fuse but CPF can.  Tableau-level `dag_fold` reduces Pauli rotations; end-to-end gate count depends on `from-tableau` synthesis (see `rot before→after` in `docs/benchmark_results.md`). |
| `qasm/bench_3q_mix.qasm`        | 3-qubit Clifford-T mix with two CX cones. Stresses the full `cpf-optimize` pipeline including the structural cleanup pass. |

## Running

```bash
make build/qsyn                          # ensure binary exists
python scripts/cpf_bench.py --qsyn build/qsyn
```

Add `--with-bqskit` if BQSKit is installed and you want a side-by-side
recompile column in the report.

## Adding a benchmark

1. Drop a `.qasm` file (QASM 2.0, no classical registers) into `qasm/`.
2. Re-run `cpf_bench.py`; the new row is appended automatically.
3. Equivalence is the must-have property -- the runner exits non-zero
   if any benchmark fails the `verify_equiv` check.  Gate-count
   regressions are reported but do not fail the run, because some
   inputs intentionally exercise expansion paths (e.g. trace-replay
   round-trip through a `naive` rotation strategy emits CX-ladders).
