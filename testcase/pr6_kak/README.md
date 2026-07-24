# `pr6_kak/` — `qcir synthesize` (KAK + ZYZ + gray-code fallback)

Demo and validation for the new `qcir synthesize` CLI.

| File | Role |
|--|--|
| `qasm/u3_general.qasm` | Single-qubit `u(π/3, π/4, π/5)`. The KAK ZYZ path recovers exactly the same `UGate` parameters; `qcir equiv` confirms unitary equivalence. |
| `qasm/h_cx_h.qasm` | `H ⊗ H` sandwiching a `CX`, which equals `CZ` up to a single-qubit Hadamard conjugation. KAK's tensor-product factorisation fails on this CZ-like input (open follow-up); the synthesise CLI falls back to the gray-code `tensor::Decomposer` and the result is still unitarily equivalent. |
| `qasm/two_qubit_random.qasm` | Non-trivial 2-qubit circuit with non-Clifford rotations on both qubits and two CX gates. Exercises every path of `two_qubit_synthesize`; gray-code fallback again kicks in and produces an equivalent circuit. |
| `dof/kak_demo.dof` | Drives all three cases, prints the synthesised gate list, and runs `qcir equiv` to verify each round-trip. |

Usage:

```bash
./build/qsyn -f testcase/pr6_kak/dof/kak_demo.dof
```

## Known limitation

The 2-qubit KAK path's sign-fixing logic is conservative — it returns
`nullopt` rather than guess when the magic-basis eigendecomposition lands
on a borderline case. The CLI surface compensates with a gray-code
fallback (more CNOTs but always correct). Follow-up work: harden the
Takagi-factorisation step so KAK succeeds for every SU(4), giving the
optimal three-CNOT centre.
