# CPF fold strategy micro-benchmark

Compares tableau **Pauli rotation count** after fold (not final RZ/T-count) on small circuits.

## Run

```bash
make -C build qsyn
python3 scripts/cpf_strategy_bench.py --qsyn build/qsyn
```

Latest numbers:

- Micro: [cpf_strategy_bench_results.md](cpf_strategy_bench_results.md)
- JKU table-17 (partial): [jku_table17_strategy_bench.md](jku_table17_strategy_bench.md) — heavy circuits skipped in `benchmark/jku_table17.manifest`

## Strategies (`qcir cpf-optimize --skip-u3cx`)

| Label | Flags |
|--|--|
| `global_r1` | `--fold-strategy global --rounds 1` |
| `staq_r1` | `--fold-strategy staq --rounds 1` |
| `no_prune_r1` | `--fold-strategy no_prune --rounds 1` |
| `dag_r1` | `--fold-strategy dag --rounds 1` (product default, one round) |
| `matching_r1` | `--fold-strategy matching --rounds 1` |
| `dag_r3` / `matching_r3` | same strategy, `--rounds 3` (retranspile loop) |

Input **kind** (from Qiskit gate names): `arbitrary` (has rx/ry/rz/u), `clifford` (H/CX only), `mixed`.

## Implementation

- `max_matching_merge` in `pauli_dag_graph.cpp` — exact on ≤24 merge edges.
- `CpfFoldStrategy` in `cpf_pipeline.hpp`.

PauliOpt is **not** wired; it uses a different IR and may insert CX for resynthesis.
