# `src/ncf` — NcfProgram library

Post-fusion view of Non-Clifford Fusion (NCF): structured blocks `[C†][R'][C]`, inspection, and commute-layer analysis.
Paper: [arXiv:2510.13573](https://arxiv.org/abs/2510.13573).
CLI lives in [`src/cmd/ncf_cmd.cpp`](../cmd/ncf_cmd.cpp); fusion itself is still `tableau optimize ncf` in `src/tableau/`.

Shell-cancellation workflows (junctions / schedule / reorder / merge-shells) were removed from the CLI — post-NCF seams rarely cancel in practice.

## Source files

| File | Role | Typical commands |
|------|------|------------------|
| [`ncf_types.hpp`](ncf_types.hpp) / [`ncf_report.cpp`](ncf_report.cpp) | Shared types + fusion-report printing. | `ncf print --summary` / `--cost` |
| [`ncf_block.hpp`](ncf_block.hpp) / [`ncf_block.cpp`](ncf_block.cpp) | One fused group: Paulis before/after, inner rotations, pivot, Clifford shells, per-block stats. | `ncf block print [--view …]` |
| [`ncf_program.hpp`](ncf_program.hpp) / [`ncf_program.cpp`](ncf_program.cpp) | Ordered collection of blocks + front Clifford; convert back to `Tableau`. | (used by convert / print) |
| [`ncf_convert.hpp`](ncf_convert.hpp) / [`ncf_convert.cpp`](ncf_convert.cpp) | Build `NcfProgram` from an NCF-shaped tableau; cost estimation; emit `Tableau` / `QCir`. | `tableau optimize ncf` (auto), `ncf from-tableau`, `ncf to-qcir` |
| [`ncf_analysis.hpp`](ncf_analysis.hpp) / [`ncf_analysis.cpp`](ncf_analysis.cpp) | Block commute graph / layers. | `ncf analyze commute` |

Manager wrapper: [`src/cmd/ncf_mgr.hpp`](../cmd/ncf_mgr.hpp).

## Command cheat sheet

```text
# create NcfProgram (auto after fusion, or manually)
tableau optimize ncf
tableau optimize ncf --overlap-priority
tableau optimize ncf --all-merges --max-cases 12
tableau optimize ncf --all-merges --overlap-priority --max-cases 12
ncf from-tableau [id]

# inspect program / costs
ncf print --summary
ncf print --cost
ncf print --blocks

# inspect one block
ncf block checkout <id>
ncf block print [--view pauli|stats|inner|layers|parity|cx-graph]

# analyze structure
ncf analyze commute

# emit / export
ncf to-qcir [-r ncf]
ncf write <path.json>
```

## `tableau optimize ncf` flags

| Flag | Meaning |
|------|---------|
| (default) | Greedy **1-qubit** NCF: peel anti-commuting **generator** pairs (then optional product), emit `[C†][R'][C]` |
| `--two-qubit` | Paper §IV-A2 **two-qubit** grouping: grading (+3 product / +1 generated) + Table III + sliding window (default **w=128**) |
| `--window W` | Override sliding-window size (`0` = paper default: 4 for 1q, 128 for 2q) |
| `--all-merges` | Enumerate many peel orders / merge-vs-split cases; each case → its own tableau ID (1q) |
| `--max-cases N` | Cap how many enumerated cases to keep (`0` = unlimited); used with `--all-merges` |
| `--overlap-priority` | Change **which** anti-pair is peeled next in **1q** mode (see below) |

**2-qubit mode notes**

- Leftover mutually commuting Paulis become **singletons** (one RZ job each).
- Conjugation folds each anti-group onto ≤2 qubits and emits **Clifford + RZ** gadgets.
- Does **not** synthesize fused 2-qubit unitaries into Clifford+T (no Synthetiq).
- Example: `NCF_dofile/H2_ncf2q.do`

### `--overlap-priority` (Paulihedral overlap)

From **Paulihedral** (ASPLOS’22): score how similar two Pauli strings are by counting qubits where both are non-`I` and carry the **same** letter (`X`/`Y`/`Z`).

In qsyn (`paulihedral_overlap` in `tableau_optimization.cpp`):

```text
overlap(P, Q) = #{ q | P_q ≠ I, Q_q ≠ I, P_q = Q_q }
```

**What the flag does for NCF**

- **Default:** among remaining terms, pick the first anti-commuting pair from the symplectic **generator** set (deterministic index order).
- **`--overlap-priority`:** consider **all** remaining anti-commuting pairs and pick the one with largest Paulihedral overlap; ties broken by a small “chain” score (overlap of the pair with other active terms), then by index.

It does **not** change conjugation math or add a cost model for Clifford gates — only the **pairing / fusion schedule**. Can be combined with `--all-merges`.

## Minimal example

```bash
qsyn -c "
  qcir read h2_pyscf.qasm
  convert qcir tableau
  tableau optimize ncf
  ncf print --summary
  ncf analyze commute
  ncf block print 0 --view layers
  ncf to-qcir -r ncf
"
```

Also see [`tests/ncf/dof/h2_ncf_program.dof`](../../tests/ncf/dof/h2_ncf_program.dof) and [`NCF_docs/NCF_branch.md`](../../NCF_docs/NCF_branch.md).
