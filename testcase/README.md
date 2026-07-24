# `testcase/` — CPF branch sandbox

Per-PR small inputs and dofiles used as sanity / regression checks for the
`continuous-phase-folding` branch. Real Catch2 tests still live under
`tests/src/`; everything here is dofile- or eyeball-friendly.

Run a dofile from the repo root after building qsyn, e.g.:

```bash
build/qsyn testcase/pr1_u3_ugate/dof/u3_read_print.dof
```

## CPF product pipeline (interactive CLI)

From the repo root, start qsyn and run either the full pipeline in one shot
or the same steps individually. The chain is:

`U3+CX` → `ZYZ+CX` → interleaved `Tableau` (optional CPF fold) → `QCir` via
Gray-synth (naive fallback for non-diagonal Pauli blocks). Gridsynth is out of
scope for this branch.

```bash
cd qsyn
./build/qsyn

# Step-by-step
qcir read your_circuit.qasm
qcir to-u3cx -r                    # QuickScan partition + QSD/KAK; opt-level 1
qcir to-u3cx --opt-level 2 -r      # + ScanningGateRemoval workflow (U3+CX)
qcir to-u3cx --opt-level 3 -r      # + resynthesis lite
qcir to-u3cx --prefer-qsearch -r   # try bqskit QSearch per block when installed
qcir to-u3cx --qsearch -r          # per-block QSearch (bqskit_block_synth.py)
qcir to-u3cx --leap -r             # per-block LEAP
qcir to-u3cx --qfast -r            # QFAST for blocks with >=4 qubits
qcir to-u3cx --device-topology -r  # gate deletion respects `device` coupling (after device read)
qcir to-u3cx --monolithic -r       # legacy whole-unitary QSD
qcir to-u3cx --u3syn --opt-level 2 -r   # full BQSKit compile()
qcir to-u3cx --bqskit -r           # alias for --u3syn
qcir to-zyz -r
qcir to-tableau --fold          # trace_replay + dag_fold (staq fold_forward + Pauli DAG + merge)
qcir from-tableau -r
qcir equiv 0

# One-shot (same as `qcir cpf-pipeline -r`; also aliased as `qcpfq` in qsynrc)
qcir read your_circuit.qasm
qcpfq                           # default: dag_fold + 3 retranspile rounds
qcpfq --rounds 5 --no-dag-fold  # legacy global_fold only, more rounds

# If the input is already rz/h/cx-native (e.g. files under benchmark/), skip
# logical U3+CX re-synthesis to avoid blowing up gate count:
qcir read testcase/benchmark/qasm/bench_2q_continuous.qasm
qcpfq --skip-u3cx
qcir equiv 0
```

Automated regression for the pipeline lives in
`testcase/pr10_pipeline/dof/pipeline_steps.dof`. Micro-benchmarks use
`scripts/cpf_bench.py` (internally passes `--skip-u3cx` to `qcir cpf-optimize`).

| Directory | PR | Description |
|--|--|--|
| `pr1_u3_ugate/` | PR-1 | Arbitrary `U`/`U3` QASM parser + ZYZ expansion |
| `pr2_angle_utils/` | PR-2 | `is_clifford_phase` / `is_zero_phase` reference table |
| `pr3_propagation_merge/` | PR-3 | Sample circuits for the four `MergeClass` cases |
| `pr4_global_fold/` | PR-4 | `trace_replay` + `cpf-global` / `cpf-full` end-to-end demo |
| `pr5_cli/` | PR-5 | `tableau optimize cpf-{merge,global,full}` + `convert qcir tableau --trace-replay` CLI exercises |
| `pr6_kak/` | PR-6 | `qcir synthesize` single-qubit ZYZ and 2-qubit KAK / gray-code fallback |
| `pr7_qsd/` | PR-7 | `qcir synthesize` on 3-qubit inputs via the QSD dispatcher |
| `pr8_qfactor/` | PR-8 | `qcir instantiate` (QFactor-lite) numerical polish on perturbed ansatz / target pairs |
| `pr9_cpf_full/` | PR-9 | `qcir cpf-optimize` / `qcir cpf-pipeline` (legacy name kept) |
| `pr10_pipeline/` | PR-10 | Product pipeline: `to-u3cx` → `to-zyz` → `to-tableau` → `from-tableau` + `qcpfq` |
| `benchmark/` | PR-11 | Micro-benchmark suite driven by `scripts/cpf_bench.py`; produces `docs/benchmark_results.md` |

Each sub-directory contains its own `README.md` enumerating files and
expected behaviour. Append future PR directories below the table above; do
not retire entries even if superseded — they double as regression material.

