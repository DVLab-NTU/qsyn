# 2-qubit NCF (paper §IV-A2, w=128) — 10 cases

Method: `tableau optimize ncf --two-qubit --window 128`  
Emit: Clifford+RZ only (no Clifford+T / Synthetiq).

| Case | nq | #Pauli | anti-groups | **singletons** | Num_unitaries | merge% | time |
|------|---:|-------:|------------:|---------------:|--------------:|-------:|-----:|
| h2_jw_four | 4 | 14 | 2 | **5** | 7 | 50.0% | 0s |
| LiH_frz_JW | 10 | 144 | 26 | **1** | 27 | 81.2% | 15s |
| Be2-JW-6 | 6 | 61 | 11 | **3** | 14 | 77.0% | 1s |
| BH-JW-10 | 10 | 275 | 52 | **5** | 57 | 79.3% | 23s |
| uccsd_10 | 10 | 664 | 136 | **0** | 136 | 79.5% | 79s |
| OH-JW10 | 10 | 275 | 52 | **5** | 57 | 79.3% | 25s |
| OH-JW12 | 12 | 630 | 113 | **9** | 122 | 80.6% | 109s |
| H2O_frz_JW_sto3g | 12 | 640 | 119 | **0** | 119 | 81.4% | 191s |
| Li2-JW14 | 14 | 669 | 129 | **7** | 136 | 79.7% | 267s |
| NH-JW-14 | 14 | 1085 | 202 | **6** | 208 | 80.8% | 528s |

All 10 status: **ok**.

## Singleton blocks (size=1; singleton_count_in_block=1)

- **h2_jw_four** (5): blocks 2–6 → idxs 2, 5, 9, 11, 13
- **LiH_frz_JW** (1): block 26 → idx 138
- **Be2-JW-6** (3): blocks 11–13 → idxs 13, 58, 59
- **BH-JW-10** (5): blocks 52–56 → idxs 13, 32, 263, 268, 274
- **uccsd_10** (0): none
- **OH-JW10** (5): blocks 52–56 → idxs 13, 32, 263, 268, 274
- **OH-JW12** (9): blocks 113–121 → idxs 60, 103, 608–612, 621, 629
- **H2O_frz_JW_sto3g** (0): none
- **Li2-JW14** (7): blocks 129–135 → idxs 659–665
- **NH-JW-14** (6): blocks 202–207 → idxs 1060, 1061, 1063, 1066, 1077, 1079

Multi-blocks have `singleton_count_in_block=0` (each anti-group is one fused 2q target).

## Artifacts

Per case under `out/ncf2q_10cases/<Case>/`:

- `<Case>_before_2qncf_clifford_rz.qasm`
- `<Case>_after_2qncf_clifford_rz.qasm`
- `blocks.txt`, `run_ncf2q.log`

CSV: `out/ncf2q_10cases/summary.csv`
