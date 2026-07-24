# JKU table-17 CPF fold strategy benchmark (partial)

Generated from checkpoint + logs. **Metric:** tableau Pauli rotations after `trace_replay` + fold (`cpf-optimize --skip-u3cx`, no `full_optimize`).

**Status:** Heavy circuits skipped by user request; remaining JKU entries not run to completion.

## Skipped (heavy / residual)

| Circuit | Reason |
|--|--|
| `adder_n118` | >900s timeout on `dag` / `no_prune`; partial: global 728→728, staq 728→572 |
| `rd73_252` | Manifest `# skip (heavy)` (~2317 T) |
| `urf2_277` | Manifest `# skip (heavy)` (~7707 T) |
| `cm42a_207` … `hwb6`, `gf2^*`, etc. | Run aborted; only partial `cm42a` below |

## Completed — JKU / QASMBench subset

| circuit | n (table) | global_r1 | staq_r1 | no_prune_r1 | dag_r1 | matching_r1 | dag_r3 | matching_r3 | best (rots after) |
|--|--|--|--|--|--|--|--|--|--|
| `adder_n28` | 28 | 168→168 | 168→132 | 168→132 | **168→66** | **168→66** | 0→0† | 0→0† | **66** (dag/matching r1) |
| `adder_n64` | 64 | 392→392 | 392→308 | 392→308 | **392→154** | **392→154** | — | — | **154** (dag/matching) |

† `dag_r3` / `matching_r3` report `0→0` because later rounds start from an already-synthesized QCir with few/no rotations in the tableau stream — not a fair “single-pass” count. Treat **r1** as the primary comparison for large circuits.

### Partial (interrupted)

| circuit | global_r1 | staq_r1 | notes |
|--|--|--|--|
| `cm42a_207` | 770→770 | 770→479 | stopped mid-run |

## Micro-benchmarks (completed earlier)

See [cpf_strategy_bench_results.md](cpf_strategy_bench_results.md).

| circuit | best after | winner |
|--|--|--|
| `bench_3q_mix` | 3 | staq / dag / matching (global stuck at 4) |
| `bench_2q_easy` | 2 | all r1 tie |
| `bench_2q_continuous`, `h_sandwich` | 1 | all r1 tie |
| `bench_2q_clifford` | 0 | dag_r3 / matching_r3 (retranspile) |

## Circuit availability (`qsyn/benchmark` vs table)

| Table name | In `qsyn/benchmark`? | Used path |
|--|--|--|
| `cm42a_207`, `cm82a_208`, `rd53_251`, `rd73_252`, `urf2_277` | Yes (`SABRE/large/`) | qsyn tree |
| `adder_n28/64/118` | No | `CPF/QASMBench/large/` |
| `gf2^4`–`gf2^7` | No (`.qc` only, no T-native QASM) | `CPF/.../arithmetic/*.qasm` |
| `hwb4_49`, `hwb5_53`, `ham7_104`, `cm152a_212` | No | `CPF/.../jku_suite/` |
| `hwb6` (7q) | No | `CPF/feynman/benchmarks/qasm/hwb6.qasm` |

## Conclusions

1. **`global_fold` alone fails on CCX-heavy adders** (`adder_n28`: 168→168) while **staq/dag** cut rotations strongly (→66 / →154).
2. **`matching` ≡ `dag` (r1)** on all finished cases — max matching did not beat greedy graph merge on these instances.
3. **`no_prune` ≡ `staq`+graph path** on adders (same as dag minus global_prune effect); global label prune did not change adder numbers in the completed runs.
4. **Multi-round (`dag_r3`)** can help when re-synthesis exposes new structure (`bench_2q_clifford`); on adders, r3 metrics are misleading once the circuit is mostly Clifford.
5. **Heavy benchmarks** (`adder_n118`, `rd73`, `urf2`) need a lite strategy set (no ablations, no r3) or much longer timeouts — excluded here.

## Reproduce (light set)

```bash
# manifest already marks heavy lines with '# skip (heavy)'
python3 scripts/cpf_strategy_bench.py \
  --qsyn build/qsyn \
  --manifest benchmark/jku_table17.manifest \
  --checkpoint docs/jku_table17_checkpoint.json \
  --out docs/jku_table17_strategy_bench.md
```
