# `scripts/` — Python helpers for the CPF / BQSKit pipeline

These scripts are the *external* counterpart to the C++ pipeline added in
PR-0 ~ PR-9.  They use Qiskit (and optionally BQSKit) to cross-check what
`qsyn` produces and to drive batch benchmarks.

## Proposed Synthesis Flow (`paulicompress/`)

Lab-facing entry point on the **`paulicompress`** branch:

```bash
make -j$(nproc)
pip install -r scripts/paulicompress/requirements.txt
# Proposed Flow (Heuristic = zero-sweep): fast / slow
python3 scripts/paulicompress/cli.py all --bench LiH --fidelity 0.99 --eps 1e-3
python3 scripts/paulicompress/cli.py run --mode fast --bench LiH --fidelity 0.99
python3 scripts/paulicompress/cli.py run --mode slow --bench LiH --fidelity 0.99
```

See [`paulicompress/README.md`](paulicompress/README.md): main path is
**Heuristic (zero-sweep)** + optional Phase Folding (slow) + Gridsynth/`qzq`.
Methods A–E and JW/BK/Parity are research extras.

## Tools

| Script | Purpose | PR |
| --- | --- | --- |
| `paulicompress/cli.py` | Proposed Flow + Methods A–E / experiment_best + phase-fold compare | paulicompress |
| `verify_equiv.py` | Exact unitary equivalence between two QASM 2.0 files (up to a single global phase). Drop-in external sibling of `qcir equiv`. | PR-10 |
| `bqskit_compare.py` | Re-compile a baseline circuit through BQSKit (U3 + CNOT target) and report gate / CNOT / depth counts side-by-side with the qsyn candidate. | PR-10 |
| `cpf_bench.py` | Drive `qcir cpf-optimize` over every `.qasm` in a directory, verify equivalence, optionally include the BQSKit recompile, and emit a Markdown report (default sink: `docs/benchmark_results.md`). | PR-11 |

## Dependencies

```bash
pip install qiskit numpy            # required by all three scripts
pip install bqskit                  # optional; only for bqskit_compare / --with-bqskit
```

Tested with `qiskit==2.0.x`.  `verify_equiv.py` and `cpf_bench.py` import
qiskit lazily so missing it only fails when actually running.

## Typical workflows

### 1. Single equivalence sanity check

```bash
python scripts/verify_equiv.py input.qasm /tmp/qsyn_out.qasm
echo "qsyn exit code: $?"
```

### 2. Per-circuit comparison with BQSKit

```bash
build/qsyn -c "qcir read input.qasm; qcir cpf-optimize -r; qcir write /tmp/out.qasm; quit -f"
python scripts/bqskit_compare.py \
    --baseline  input.qasm \
    --candidate /tmp/out.qasm \
    --opt-level 1
```

### 3. Batch benchmark + Markdown report

```bash
python scripts/cpf_bench.py \
    --qsyn   build/qsyn \
    --inputs testcase/benchmark/qasm \
    --out    docs/benchmark_results.md
```

The runner writes the Markdown table to `--out` *and* prints it to stdout
so it can be embedded in PR descriptions or CI logs directly.

## How verification is wired

Both `verify_equiv.py` and `cpf_bench.py` compare full dense unitaries
via Qiskit's `Operator(QuantumCircuit)`.  Two circuits are considered
equivalent when there exists a single complex phase `e^{i*phi}` such that

```
|| U_1 - e^{i*phi} U_2 ||_F  <=  tol * max(1, ||U_1||_F)
```

The phase is anchored on the largest-magnitude entry of `U_2`, mirroring
qsyn's internal Tableau-based check.  Tolerance defaults to `1e-7`,
matching the `qcir equiv` precision floor.

## Limitations

* All three scripts assume QASM 2.0 input.  Higher-level Qiskit IR
  features (e.g. classical registers, control flow) are out of scope.
* BQSKit's default `compile(...)` API is used; finer-grained
  `CompilationTask` orchestration is intentionally not exposed here.
* `cpf_bench.py` shells out to qsyn with `-c "...;quit -f"` and parses
  the QASM it writes back to a temporary file.  Failures inside qsyn
  surface as raised `RuntimeError` with the captured stdout/stderr.
