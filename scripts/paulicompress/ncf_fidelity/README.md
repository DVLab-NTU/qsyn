# NCF synthesis-fidelity audit (lab packaging)

Helps audit **NCF paper pathway fidelity** (arXiv:2510.13573): for each fused
unitary (1q U3/ZYZ or 2q group), compute process fidelity
\(F_k = |\mathrm{Tr}(U^\dagger V)|/d\), then

\[
F \approx \prod_k F_k
\]

(aligned / direct / worst variants for Synthetiq 2q).

## What this is / is not

| Included | Not included |
|----------|--------------|
| Fuse → synth → product \(F\) | Full NCF paper T-count reproduction as the lab default |
| gridsyn / ncf1 (pygridsynth stand-in for Trasyn **fidelity**) | Vendored Trasyn / Rustiq binaries |
| ncf2 via optional Synthetiq binary | Shipping the Synthetiq tree in git |

## CLI

```bash
# single-qubit pathways (needs: pip install pygridsynth mpmath numpy)
python3 scripts/paulicompress/cli.py ncf-fidelity \
    --bench LiH --methods gridsyn ncf1

# two-qubit pathway (needs Synthetiq binary; see below)
python3 scripts/paulicompress/cli.py ncf-fidelity \
    --bench LiH --methods ncf2 --syn-time 4

# or drive the module directly
python3 scripts/paulicompress/ncf_fidelity/run_ncf_repro.py \
    --benchmarks LiH Ising-2D-30 --methods gridsyn ncf1
python3 scripts/paulicompress/ncf_fidelity/run_fidelity_audit.py \
    --benchmarks LiH H2O --syn-time 4
```

Outputs under `scripts/paulicompress/ncf_fidelity/results/`:
- `summary_ncf_fidelity.csv` — `F_approx` / `F_direct` / `F_worst`
- `summary_ncf_tablev.csv` — gate metrics vs paper Table V refs

## Pauli inputs

Default: `scripts/pca_compress/reproduction_bundle_proposed_flow/01_original_benchmarks/<Bench>/<Bench>.pauli`

Override:

```bash
python3 scripts/paulicompress/cli.py prepare --bench LiH --out out/01_original_benchmarks
python3 scripts/paulicompress/cli.py ncf-fidelity --bench LiH \
    --bench-root out/01_original_benchmarks --methods gridsyn ncf1
# or: export NCF_BENCH_ROOT=out/01_original_benchmarks
```

## Synthetiq (ncf2 only)

Point at a built Synthetiq tree:

```bash
export NCF_SYNTHETIQ_DIR=/path/to/synthetiq   # expects bin/main
```

If unset, the code also looks for the local experiment checkout:

`scripts/pca_compress/results_pc_axis/pipeline_v8/ncf_repro/tools/synthetiq`

## Formula reminder

- **1q**: each fused group → local \(2\times2\) target; \(d=2\)
- **2q**: each fused group → local \(4\times4\) target; \(d=4\); Synthetiq may match under qubit permutation → use **aligned** \(F_k\)
