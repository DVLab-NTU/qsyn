# Baselines for Proposed Flow front-end stages

Stage names (use these in code / CLI, **not** slide shorthand w/ / w/o):

| Stage | Meaning | Location |
|-------|---------|----------|
| `hamiltonian` | Paulihedral / lattice Trotter Pauli list | `reproduction_bundle…/01_original_benchmarks/` or `cli.py prepare` |
| `after_phase_fold` | After slow CPF (`to-zyz` → `to-tableau --fold`) | `after_phase_fold/*.pauli` |
| mapped H (JW/BK/Parity) | Same fermionic H, three qubit mappings | `ham_cache/*.json` + `cli.py prepare-mapping` |

## `after_phase_fold/`

Frozen slow-CPF extracts (default BK-style NCF lists):

- `LiH.pauli` — from `LiH_pyscf_bk` fold (544 rotations)
- `H2O.pauli`, `N2.pauli`, `H2S.pauli`, `CO2.pauli`

## `ham_cache/`

Shipped fermionic→qubit coefficient caches for Track B mappings
(`{Bench}_{hf|full}_{jordan_wigner|bravyi_kitaev|parity}.json`).

Used by `hamiltonian_mappings.py` / `cli.py prepare-mapping` without PySCF.
Regenerate with optional `qiskit-nature` + `pyscf` if a cache file is missing.
