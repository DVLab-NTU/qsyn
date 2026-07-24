# Staq Pauli-sum / rotation folding ↔ qsyn mapping

Audit of [softwareQinc/staq](https://github.com/softwareQinc/staq) (cloned at audit time) and
the Python CPF prototype (`~/CPF`) against qsyn `continuous-phase-folding`.

## Staq core representation

| Staq (`include/staq/gates/channel.hpp`) | qsyn equivalent |
|--|--|
| `Gatelib::Pauli` — sparse multi-qubit Pauli string | `experimental::PauliProduct` (`tableau/pauli_rotation.hpp`) |
| `Gatelib::Rotation` — `exp(i·θ/2 · P)` with `commute_left(Clifford)` | `experimental::PauliRotation` |
| `Gatelib::Clifford` — symplectic tableau multiply | `experimental::StabilizerTableau` |
| `circuit_callback` — ordered list of `Clifford \| Rotation \| Uninterp` | interleaved `experimental::Tableau` (`trace_replay`) |
| `Rotation::try_merge` / `commutes_with` | `cpf::classify_segment` + `merge_rotations` |
| `RotationOptimizer::fold_forward` (visitor on QASM AST) | `cpf::propagation_merge` + `global_fold` |

Staq **rotation folding** (`optimization/rotation_folding.hpp`) walks a basic block,
accumulates Clifford prefix `current_clifford_`, pushes rotations as
`(rotation_info, R.commute_left(C))`, then `fold_forward` merges via `try_merge` when
commuting. qsyn's interleaved tableau + `propagation_merge` is the same algebra with
`StabilizerTableau::apply` for conjugation.

## Python CPF global mode (not identical to staq fold_forward)

| Python (`cpf_global.py`) | qsyn (this branch) |
|--|--|
| `extract_pe_stream_tableau` → Clifford/Pauli stream | `cpf::trace_replay` → `Tableau` |
| `_stream_pauli_terms` — sum angles per **Pauli label** | `pauli_dag::accumulate_global_terms` |
| `fold_terms` — drop zero / Clifford classes | `pauli_dag::fold_terms` |
| `resynthesize_global_stream` — drop pruned labels, keep original angles | `pauli_dag::prune_globally_removed_rotations` |
| `max_rounds` retranspile loop | `CpfPipelineOptions::max_rounds` |
| `continuous_phase_fold` local greedy | `cpf::propagation_merge` inside `global_fold` / `dag_fold` |

## Pauli DAG (Cole thesis / staq Pauli-sum view)

A **Pauli DAG** orders nodes `R_P(θ)` with edges for commutation / conjugation through
Cliffords. Staq implements this implicitly as a **linear** circuit callback (not an
explicit graph class). qsyn `PauliDag` is that linear stream plus:

1. **Global label accumulation** (Python `cpf_global`) — multiset of Pauli labels.
2. **Local ordered merge** (staq `fold_forward`, qsyn `propagation_merge`).

Mapping for future graph-based merges (PauliOpt / DAG merging theorem):

| DAG concept | qsyn hook |
|--|--|
| Node `R_P(θ)` | `PauliDagEntry::Rotation` |
| Clifford edge / layer | `PauliDagEntry::Clifford` (`StabilizerTableau`) |
| Mergeable pair (same P after conjugation) | `cpf::classify_segment` |
| Global class prune | `pauli_dag::global_prune_pass` |

## API surface added in qsyn

- `timeline.hpp` — `build_timeline` / `rebuild_tableau` flat Clifford+rotation stream
- `rotation_merge.hpp` — staq `try_merge` / `commute_left` on `PauliRotation`
- `staq_fold.hpp` — `staq_fold_timeline` (`fold_forward` sweeps)
- `pauli_dag_graph.hpp` — explicit nodes, dependency edges, merge edges; `graph_component_merge`
- `dag_fold(Tableau&)` — staq fold → graph edge fold → global prune → `global_fold`
- `qcir::CpfPipelineOptions::use_dag_fold`, `max_rounds`

## Out of scope (unchanged)

- Staq AST visitor / QASM round-trip
- PauliOpt `PhaseCircuit` / annealing
- Gridsynth, FastTODD (external / other branches)
