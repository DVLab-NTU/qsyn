# paulicompress (Proposed Synthesis Flow)

Open-source helper branch for lab members: turn Hamiltonian-simulation
benchmarks into Clifford+T via **PauliCompress (heuristic)** + **Gridsynth** +
**qsyn `qzq`**.

## Routes

| mode | pipeline |
|------|----------|
| **fast** | Hamiltonian `.pauli` → `tableau read-pauli` → **`pauli-compress -l`** → Gridsynth → **`qzq`** |
| **slow** | Hamiltonian `.qasm` → **`to-zyz`** → **`to-tableau --fold`** (trace_replay → **dag_fold** → collapse) → **`pauli-compress -l`** → Gridsynth → **`qzq`** |

`pauli-compress` is the in-tree heuristic (lossless merge/cancel, then greedy
Clifford-snap under an L2 budget). Default budget from fidelity target:

```
B_L2 = sqrt(-2 ln F*)     # e.g. F*=0.99 → B_L2 ≈ 0.1418
```

## Setup

```bash
git clone <this-repo> -b paulicompress
cd qsyn
make -j$(nproc)                          # builds CPF + pauli-compress into ./build/qsyn
pip install pygridsynth mpmath           # Gridsynth
# molecule pickles already under scripts/pca_compress/benchmark_circuit/source/
```

## One-liner (recommended)

```bash
# prepare LiH + run both fast and slow (F*=0.99, Gridsynth ε=1e-3)
python3 scripts/paulicompress/cli.py all --bench LiH --fidelity 0.99 --eps 1e-3
```

Outputs:

```
out/01_original_benchmarks/LiH/{LiH.pauli,LiH.qasm}
out/flow/fast/LiH/{after_pauli_compress.qasm,after_gridsynth.qasm,clifford_t.qasm,summary.json}
out/flow/slow/LiH/...
```

## Step-by-step

```bash
# 1) Paulihedral / lattice → 01_original layout
python3 scripts/paulicompress/cli.py prepare \
    --out out/01_original_benchmarks \
    --bench LiH Ising-2D-30

# 2a) fast
python3 scripts/paulicompress/cli.py run --mode fast \
    --bench LiH --bench-root out/01_original_benchmarks \
    --fidelity 0.99 --eps 1e-3 --out out/flow

# 2b) slow
python3 scripts/paulicompress/cli.py run --mode slow \
    --bench LiH --bench-root out/01_original_benchmarks \
    --fidelity 0.99 --eps 1e-3 --out out/flow
```

Smoke without QCO:

```bash
python3 scripts/paulicompress/cli.py run --mode fast \
    --bench Ising-2D-30 --bench-root out/01_original_benchmarks \
    --fidelity 0.99 --eps 1e-3 --skip-qzq --out out/flow
```

## Equivalent qsyn dofiles (for debugging)

**fast**

```text
tableau read-pauli "LiH.pauli"
tableau optimize pauli-compress -l 0.1418
convert tableau qcir
qcir write "after_pauli_compress.qasm"
```

**slow**

```text
qcir read "LiH.qasm"
qcir to-zyz -r
qcir to-tableau --fold -r
tableau optimize pauli-compress -l 0.1418
convert tableau qcir
qcir write "after_pauli_compress.qasm"
```

Then Gridsynth (Python) + QCO:

```text
qcir read "after_gridsynth.qasm"
qzq
qcir write "clifford_t.qasm"
```

## Layout notes

- Benchmark sources: `scripts/pca_compress/benchmark_circuit/` (Paulihedral pickles + generator).
- Reference packaged circuits (optional): `scripts/pca_compress/reproduction_bundle_proposed_flow/`.
- Heavy experiment dumps under `scripts/pca_compress/results_pc_axis/` are **not** part of this public flow (gitignored).
