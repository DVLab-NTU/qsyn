# `pr7_qsd/` — `qsd::synthesize` dispatcher demo

Validation for PR-7's recursive synthesis dispatcher. The current scope
covers correct circuit emission for arbitrary qubit counts using KAK / ZYZ
for the base cases and the gray-code decomposer as a placeholder for the
`n >= 3` recursion (until the cosine-sine decomposition lands).

| File | Role |
|--|--|
| `qasm/three_qubit_ghz.qasm` | Classic GHZ-prep (H + 2 CX). All Clifford. `qcir synthesize` should return a 3-qubit circuit unitarily equivalent to the input. |
| `qasm/three_qubit_mixed.qasm` | Mixed non-Clifford rotations and CX gates. Exercises the gray-code fallback inside `qsd::synthesize`. |
| `dof/qsd_demo.dof` | Runs both cases through `qcir synthesize` and verifies via `qcir equiv`. |

Usage:

```bash
./build/qsyn -f testcase/pr7_qsd/dof/qsd_demo.dof
```

## Future work

`qsd::synthesize(n>=3)` currently logs an info message and forwards to
`tensor::Decomposer`. The hooks to swap in a real CSD-based recursion are
in `src/tensor/qsd.cpp`; the header doc lists the exact decomposition
shape we plan to emit:

```
U = (V_1 ⊕ V_2) * CS(theta_k) * (W_1 ⊕ W_2)^dagger
```
