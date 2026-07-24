# PR-5 `tableau optimize cpf-*` CLI demo

Exercises the three new sub-commands added in PR-5:

* `tableau optimize cpf-merge`   — single propagation pass.
* `tableau optimize cpf-global`  — propagation + within-block fusion to fixpoint.
* `tableau optimize cpf-full`    — `cpf-global` followed by `full_optimize`
                                   (`tmerge + hopt + ToddPhasePolynomialOptimizationStrategy`).

It also covers the new `convert qcir tableau --trace-replay` flag, which
preserves the original Clifford / rotation segmentation that
`propagation_merge` and `global_fold` rely on.

## Files

| Path | Description |
| --- | --- |
| `qasm/cpf_cli_demo.qasm` | 2-qubit input with an `RZ - H - RZ` ladder on q[0] and a `RX - CX - RX` pair on q[1]. Designed so that *both* propagation-merge and within-block fusion fire. |
| `dof/cpf_cli_demo.dof`   | Dofile that loads the QASM, builds three tableau snapshots (one per CPF stage) and prints the resulting `(n_cliffords, n_pauli_rotations)` for each. |

## Expected output

```
Tableau (2 qubits, 3 Clifford segments, 4 Pauli rotations)     # baseline
Tableau (2 qubits, 2 Clifford segments, 3 Pauli rotations)     # after cpf-merge
Tableau (2 qubits, 2 Clifford segments, 3 Pauli rotations)     # after cpf-global
Tableau (2 qubits, ? Clifford segments, ? Pauli rotations)     # after cpf-full
```

The `?` line depends on the Clifford normal-form chosen by
`full_optimize`; both segment count and rotation count typically drop
further once the structural pipeline runs.
