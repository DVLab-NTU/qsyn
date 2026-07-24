# paulicompress — thesis core Proposed Synthesis Flow

Open-source lab entry for
*A PCA-Inspired Scalable Pauli Rotation Minimization Algorithm…*
(Ch. 3 flow + Ch. 4 Heuristic compress + Ch. 5 Gridsynth/`qzq`).

## Proposed Flow (main path — matches thesis slide)

```
Hamiltonian Circuits
        │
        ├──────────────► Phase Folding ──┐   (slow only)
        │                                │
        └────────────────────────────────┼──► Pauli Compression (Heuristic = zero-sweep)
                                         │
                                         ▼
                              Gridsynth → Qsyn QCO (qzq) → Clifford+T
```

| Mode | Phase Folding | Pauli Compression | Backend |
|------|---------------|-------------------|---------|
| **fast** | no | Heuristic (`zero-sweep`) | Gridsynth + `qzq` |
| **slow** | yes (`to-zyz` → `to-tableau --fold`) | Heuristic (`zero-sweep`) | Gridsynth + `qzq` |

**Methods A–E** are optional research tools (ablations / grids), not the default Proposed Flow compress.

## Setup

```bash
git clone <repo> -b paulicompress && cd qsyn
make -j$(nproc)
pip install -r scripts/paulicompress/requirements.txt
# optional (regen ham_cache only): pip install qiskit-nature pyscf
```

## Main commands

```bash
# prepare Hamiltonian .pauli / .qasm
python3 scripts/paulicompress/cli.py prepare --out out/01_original_benchmarks --bench LiH

# Proposed Flow — fast (no Phase Folding)
python3 scripts/paulicompress/cli.py run --mode fast --bench LiH \
    --bench-root out/01_original_benchmarks --fidelity 0.99 --eps 1e-3

# Proposed Flow — slow (with Phase Folding)
python3 scripts/paulicompress/cli.py run --mode slow --bench LiH \
    --bench-root out/01_original_benchmarks --fidelity 0.99 --eps 1e-3

# both modes (default compress = zero-sweep Heuristic)
python3 scripts/paulicompress/cli.py all --bench LiH --fidelity 0.99 --eps 1e-3

# same stages via composable pipeline (defaults also Heuristic)
python3 scripts/paulicompress/cli.py pipeline \
    --pauli out/01_original_benchmarks/LiH/LiH.pauli \
    --compress zero-sweep --fidelity 0.99 --gridsynth --qzq
python3 scripts/paulicompress/cli.py pipeline \
    --pauli out/01_original_benchmarks/LiH/LiH.pauli \
    --qasm  out/01_original_benchmarks/LiH/LiH.qasm \
    --phase-fold --compress zero-sweep --fidelity 0.99 --gridsynth --qzq
```

`--compress` default on `run` / `all` / `pipeline`: **`zero-sweep`** (Heuristic).  
Optional: `none` | `methods-ae` | `cpp-l2`.

## Research / exploration (not the main slide path)

```bash
# Heuristic-only multi-F* query
python3 scripts/paulicompress/cli.py zero-sweep \
    --pauli out/01_original_benchmarks/LiH/LiH.pauli --tiers 0.999 0.99 0.9

# Methods A–E grid + experiment_best
python3 scripts/paulicompress/cli.py methods-sweep \
    --bench LiH --stage hamiltonian --tiers 0.999 0.99 0.9
python3 scripts/paulicompress/cli.py compare-phase-fold --bench LiH H2O N2 H2S CO2

# JW / BK / Parity mapping ablation
python3 scripts/paulicompress/cli.py prepare-mapping --bench LiH \
    --mapping jordan_wigner bravyi_kitaev parity
python3 scripts/paulicompress/cli.py mapping-compress --bench LiH \
    --mapping jw bk parity --tiers 0.999 0.99 0.9

# Opt into A–E on the full CT pipeline when exploring
python3 scripts/paulicompress/cli.py run --mode fast --bench LiH \
    --compress methods-ae --fidelity 0.99

# NCF pathway synthesis-fidelity audit (∏ |Tr|/d per fused unitary)
python3 scripts/paulicompress/cli.py ncf-fidelity \
    --bench LiH --methods gridsyn ncf1
```

See [`ncf_fidelity/README.md`](ncf_fidelity/README.md).

## Thesis coverage

| Piece | In branch? | Role |
|---|---|---|
| Proposed Flow fast/slow + Heuristic + Gridsynth/`qzq` | **yes — main** | `run` / `all` / `pipeline` |
| Methods A–E / `experiment_best` | **yes — research** | `methods-sweep` / `--compress methods-ae` |
| JW/BK/Parity mappings | **yes — research** | `prepare-mapping` / `mapping-compress` |
| NCF pathway product-fidelity audit | **yes — research** | `ncf-fidelity` |
| HYBRID / RECURSIVE / full NCF fusion stack | **partial** | fidelity audit only; no Trasyn/Synthetiq vendored |

## Developer modules

| Module | Role |
|--------|------|
| `zero_sweep.py` | Heuristic F-cost compress (**default Proposed Flow**) |
| `flow.py` | fast/slow + composable `run_pipeline` |
| `methods_ae.py` / `experiment_best.py` | Methods A–E research grid |
| `hamiltonian_mappings.py` | JW/BK/Parity (+ `baselines/ham_cache/`) |
| `baselines.py` | `hamiltonian` / `after_phase_fold` loaders |
| `gridsynth_qco.py` | Gridsynth + `qzq` |
| `ncf_fidelity/` | NCF fused-unitary product fidelity (`∏ F_k`) |

## Layout

- Lab CLI: `scripts/paulicompress/`
- Benchmark sources: `scripts/pca_compress/benchmark_circuit/`
- Reference bundle: `scripts/pca_compress/reproduction_bundle_proposed_flow/`
