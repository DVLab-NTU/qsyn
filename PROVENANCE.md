# Provenance — qsyn (NCF branch)

| Tag | Meaning |
|-----|---------|
| **UPSTREAM** | DVLab-NTU/qsyn baseline |
| **OURS** | NCF feature work added locally |
| **MODIFIED** | Upstream file we changed for NCF |

- Remote: `https://github.com/DVLab-NTU/qsyn.git`
- Branch: `Non-Clifford-Fusion`

## OURS

- `src/ncf/` — `NcfProgram`, blocks, analysis, convert, report
- `src/cmd/ncf_cmd.cpp`, `src/cmd/ncf_mgr.hpp`
- `src/tableau/pauli_terms_io.{cpp,hpp}`
- `docs/NCF_PROGRAM.md`, `docs/ALGORITHMS_AND_LIMITATIONS.md`
- `NCF_dofile/` — H₂ fixtures
- `scripts/export_pyscf_terms.py`
- `run_pauliopt_h2.py`
- `tests/ncf/`
- Sample QASM: `h2_ncf.qasm`, `h2_pyscf.qasm`, …

## MODIFIED (upstream glue)

- `src/cmd/tableau_cmd.{cpp,hpp}`
- `src/convert/tableau_to_qcir.cpp`
- `src/qcir/qcir.hpp`, `qcir_action.cpp`, `qcir_writer.cpp`
- `src/qsyn/qsyn_helper.{cpp,hpp}`, `qsyn_main.cpp`
- `src/tableau/tableau.hpp`, `tableau_optimization.{cpp,hpp}`
- `src/util/phase.hpp`

## UPSTREAM

Everything else under `src/` (qcir, zx, duostra, …), `vendor/`, cmake, docker.

NCF usage docs: [docs/NCF_PROGRAM.md](docs/NCF_PROGRAM.md).
Experiment sandbox that drives this binary: `/home/chenying/Hamiltonian Simulation`.
