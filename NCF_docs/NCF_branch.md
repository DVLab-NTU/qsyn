# NcfProgram — interactive NCF block structure in qsyn

`NcfProgram` is a first-class qsyn object (managed like `Tableau` / `QCir`) that holds Non-Clifford Fusion (NCF) block metadata for checkout and inspection — without requiring JSON export as the primary workflow.

Paper: [arXiv:2510.13573](https://arxiv.org/abs/2510.13573) (Non-Clifford Fusion).  
File map for `src/ncf/` (what each source does + matching commands): [`src/ncf/README.md`](../src/ncf/README.md).

## Quick start (H₂ example)

```bash
# From PySCF-exported terms
python3 scripts/export_pyscf_terms.py --preset h2 -o /tmp/h2_jw.json

qsyn -c "
  tableau from-terms /tmp/h2_jw.json --use-coeff
  tableau optimize collapse
  tableau optimize ncf
  ncf print --summary
  ncf analyze commute
  ncf block print --view layers
"
```

Or use inline Pauli terms:

```bash
qsyn -c "tableau from-pauli --terms ZIII:0.137 ...; tableau optimize collapse; tableau optimize ncf"
```

After `tableau optimize ncf`, qsyn automatically builds a focused `NcfProgram` and prints a fusion report (Pauli term count, block count, gate costs before/after).

## Layered block views

| Layer | Content | Query |
|-------|---------|-------|
| L1 | `pauli_before` / `pauli_after` | `ncf block print --view pauli` |
| L2 | 3-subtableau block `[C†][R′][C]` | `ncf block print --view stats` |
| L3 | Shell parity + CX graph | `ncf block print --view parity` / `--view cx-graph` |
| L4 | CX / H-S layers + inner rotations | `ncf block print --view layers` / `--view inner` |

Inner rotations are stored as `vector<PauliRotation>`; synthesis to `RZ`, `H`, `S`, `S†` uses `NcfMergePauliRotationsSynthesisStrategy`.

## Commands

### Build / emit

| Command | Description |
|---------|-------------|
| `tableau optimize ncf` | Run NCF fusion; auto-create `NcfProgram` |
| `tableau optimize ncf --overlap-priority` | Same fusion, but pick anti-commuting pairs by **Paulihedral** letter overlap first (ASPLOS’22 metric); see below |
| `tableau optimize ncf --all-merges [--max-cases N]` | Enumerate merge cases into multiple tableau IDs |
| `ncf from-tableau [id]` | Build from NCF-shaped tableau |
| `ncf to-qcir [-r ncf\|naive]` | Synthesize QCir |

### `--overlap-priority` (Paulihedral)

[Paulihedral](https://dl.acm.org/doi/10.1145/3503222.3507719) (ASPLOS’22) measures Pauli-string similarity by **letter overlap**: number of qubits where both Paulis are non-identity and equal (`X`/`Y`/`Z` match).

In NCF partitioning this becomes the pair-selection heuristic:

- **Default:** peel the first anti-commuting **generator** pair.
- **`--overlap-priority`:** among all remaining anti-commuting pairs, peel the max-overlap pair (tie-break: chain score vs other active terms, then indices).

Only the fusion **schedule** changes; conjugation / `[C†][R'][C]` emission is unchanged. Details: [`src/ncf/README.md`](../src/ncf/README.md).

### Inspect

| Command | Description |
|---------|-------------|
| `ncf print [--summary\|--cost\|--blocks]` | Fusion report + optional block table |
| `ncf block checkout <id>` | Focus block |
| `ncf block print [--view pauli\|inner\|layers\|stats\|parity\|cx-graph]` | Block details |
| `ncf analyze commute` | Commute layers on L1 Pauli terms |

### Export (secondary)

| Command | Description |
|---------|-------------|
| `ncf write <path.json>` | Serialize program metadata to JSON |

> Note: shell-cancellation tools (`print --graph`, `analyze junctions/schedule`, `reorder`, `merge-shells`, `sync-tableau`) were removed — post-NCF adjacent shells rarely cancel.

## PySCF terms file format

JSON (recommended):

```json
{
  "version": 1,
  "molecule": "H2",
  "basis": "sto-3g",
  "encoding": "jordan_wigner",
  "n_qubits": 4,
  "terms": [
    {"id": 0, "pauli": "ZIII", "coeff": 0.1371657294, "angle": "0.1371657294"}
  ]
}
```

Text `.terms` format:

```
# n_qubits=4
0 ZIII coeff=0.137 angle=0.137
```

Load with: `tableau from-terms <file> [--use-coeff] [--scale 1.0]`

## Data model

- `NcfMgr` stores `unique_ptr<NcfProgram>` (like `TableauMgr`)
- `NcfProgram` owns `unique_ptr<NcfBlock>` blocks, execution `order`, lazy `NcfAnalysisCache`
- `NcfBlock` holds L1 Pauli snapshots, L2 inner `PauliRotation`s, L3 `NcfShellSide` (Clifford ops + lazy parity/CX graph)
