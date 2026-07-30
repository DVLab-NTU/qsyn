# NCF dofiles

Example qsyn scripts for Non-Clifford Fusion (NCF) on the H2 Hamiltonian circuit.
Paper: [arXiv:2510.13573](https://arxiv.org/abs/2510.13573).
Run from the **repo root** so relative paths resolve (`h2_pyscf.qasm`, `NCF_dofile/...`).

```bash
# example
qsyn -f NCF_dofile/H2.do
qsyn -f NCF_dofile/H2_all_merges_cases.do
qsyn -f NCF_dofile/H2_ncf2q.do
```

## Dofiles

| File | Description |
|------|-------------|
| [`H2.do`](H2.do) | Default single-path **1-qubit** NCF on `h2_pyscf.qasm`: convert to tableau, `tableau optimize ncf`, emit with `-r ncf`, write `h2_ncf.qasm`. Also prints Clifford form / depth for inspection. |
| [`H2_all_merges_cases.do`](H2_all_merges_cases.do) | Enumerates NCF merge variants via `tableau optimize ncf --all-merges --max-cases 12`, then converts the first four cases and writes `NCF_dofile/h2_case{0..3}.qasm`. |
| [`H2_ncf2q.do`](H2_ncf2q.do) | Paper **2-qubit** NCF (`--two-qubit --window 128`) on `tests/ncf/data/h2_jw_sto3g.json`. Saves Clifford+RZ **before** (`h2_before_2qncf_clifford_rz.qasm`) and **after** (`h2_after_2qncf_clifford_rz.qasm`). No Clifford+T synthesis of 2q blocks. |

Related fusion flag (not used in these dofiles by default): `tableau optimize ncf --overlap-priority` picks anti-commuting pairs by **Paulihedral** letter overlap (ASPLOS’22) instead of generator order. See [`src/ncf/README.md`](../src/ncf/README.md).

## Generated / companion artifacts

| File | Description |
|------|-------------|
| `h2_pyscf.qasm` (repo root) | Input circuit (PySCF H2 terms); required by both dofiles. |
| [`H2_ncf.qasm`](H2_ncf.qasm) | Example output from the default NCF path (`H2.do`). |
| [`h2_case0.qasm`](h2_case0.qasm) … [`h2_case3.qasm`](h2_case3.qasm) | Circuits from the first four `--all-merges` cases. |
| [`h2_before_2qncf_clifford_rz.qasm`](h2_before_2qncf_clifford_rz.qasm) | Naive Clifford+phase circuit before 2q NCF. |
| [`h2_after_2qncf_clifford_rz.qasm`](h2_after_2qncf_clifford_rz.qasm) | Clifford+RZ after 2q NCF conjugation (ungrouped RZ gadgets; no Clifford+T). |

See also [`src/ncf/README.md`](../src/ncf/README.md) (library + CLI) and [`NCF_docs/README_NCF.md`](../NCF_docs/README_NCF.md).
