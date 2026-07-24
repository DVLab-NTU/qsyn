# `pr4_global_fold/` — `trace_replay` + `cpf-global` + `cpf-full` demo

End-to-end demo for PR-4 (interleaved tableau builder + global fold) and
PR-5 (CLI integration of the CPF passes).

| File | Role |
|--|--|
| `qasm/h_sandwich_pair.qasm` | `rx ; H ; rz ; H ; rx` on one qubit. After `cpf-global`, the three rotations fold across the two H Cliffords into a single `X(143π/315)` rotation -- exact rational arithmetic. |
| `qasm/cx_x_target_cancel.qasm` | `rx(π/8) q[1] ; CX(0,1) ; rx(-π/8) q[1]`. `I⊗X` commutes with `CX` so the two rotations cancel completely. |
| `qasm/cx_z_pair.qasm` | `rz(π/8) q[1] ; CX ; rz(-π/8) q[1]`. `CX (I⊗Z) CX = ZZ`, so `classify_segment` returns `blocked` and the rotations are correctly *not* merged. |
| `qasm/cx_x_sign_flip.qasm` | `rz q[0] ; CX ; rz q[0]`. `Z⊗I` is invariant under `CX`, so the rotations merge to `Z(π/4)` on `q[0]`. |
| `dof/cpf_global_demo.dof` | Drives all four cases through `convert qcir tableau --trace-replay` + `tableau optimize cpf-global` and prints the tableau before / after. Also includes a comparison step that runs `cpf-full` (cpf-global + qsyn's existing `full_optimize` pipeline) against the default `to_tableau` builder. |

Usage:

```bash
./build/qsyn -f testcase/pr4_global_fold/dof/cpf_global_demo.dof
```
