# paulicompress — thesis core Proposed Synthesis Flow

Open-source lab entry for the **core** of
*A PCA-Inspired Scalable Pauli Rotation Minimization Algorithm…*
(Ch. 3 flow + Ch. 4 zero-sweep + Ch. 5 Gridsynth/`qzq`).

## Thesis coverage (this branch)

| Thesis piece | In branch? | How |
|---|---|---|
| Ch.3 Proposed Flow (fast/slow) | **yes** | `cli.py run` / `all` |
| Ch.4 zero-sweep F-cost + prefix multi-F* | **yes** | `cli.py zero-sweep` (default compress) |
| Ch.5 Gridsynth ε + `qzq` QCO | **yes** | wired in `flow.py` |
| 13 NCF benchmarks prepare | **yes** | `cli.py prepare` + pickles |
| Packaged proposed circuits (check) | **yes** | `reproduction_bundle_proposed_flow/` |
| Methods A–E / HYBRID grid sweeps | **no** | not required for core claim |
| Track B mapping ablation | **no** | |
| NCF fusion baseline repro | **no** | |
| Full Table 6.x recompute | **partial** | re-run core flow; tables need extra drivers |

Everything this CLI can run maps to the thesis **proposed** stack (not external NCF-fusion experiments).

## Routes

| mode | pipeline |
|------|----------|
| **fast** | `.pauli` → **zero-sweep (F*)** → Gridsynth → **`qzq`** |
| **slow** | `.qasm` → **to-zyz** → **to-tableau --fold** (PauliDAG) → **zero-sweep** → Gridsynth → **`qzq`** |

Optional: `--compress cpp-l2` uses Qsyn `pauli-compress -l` (L2 surrogate; not the thesis F-cost planner).

## Setup

```bash
git clone <repo> -b paulicompress && cd qsyn
make -j$(nproc)
pip install -r scripts/paulicompress/requirements.txt
```

## Commands

```bash
# 1) multi-F* zero-sweep only (Stage 1–3 of the heuristic slide)
python3 scripts/paulicompress/cli.py prepare --out out/01_original_benchmarks --bench LiH
python3 scripts/paulicompress/cli.py zero-sweep \
    --pauli out/01_original_benchmarks/LiH/LiH.pauli \
    --tiers 0.999 0.99 0.9

# 2) end-to-end proposed flow (fast + slow)
python3 scripts/paulicompress/cli.py all --bench LiH --fidelity 0.99 --eps 1e-3

# 3) single mode
python3 scripts/paulicompress/cli.py run --mode fast --bench LiH \
    --bench-root out/01_original_benchmarks --fidelity 0.99 --eps 1e-3
```

## Layout

- CLI / zero-sweep: `scripts/paulicompress/`
- Benchmark sources: `scripts/pca_compress/benchmark_circuit/`
- Reference bundle: `scripts/pca_compress/reproduction_bundle_proposed_flow/`
- Experiment dumps (`results_pc_axis/`) stay gitignored
