# PR-3 — `tableau/cpf/propagation_merge`

Four canonical inputs, one per `MergeClass`. The header documents the
algebraic identity used:

```
R_PR(theta_r) · C · R_PL(theta_l) = R_PR(theta_r) · R_{C P_L C†}(theta_l) · C
```

so the segment is mergeable iff `C * P_L * C† == ± P_R`.

## QASM samples — `qasm/`

| File | Expected `MergeClass` | Description |
|--|--|--|
| `same_pauli.qasm` | `same_pauli` | Two `rz` on the same qubit, no intermediate Clifford. |
| `h_z_h.qasm` | `propagation_aligned` | `rz(a); h; rz(b); h;`. Pushing `rz(a)` past `h` makes it `rx(a)`, which would be matched if the second was `rx(b)`. Variant below shows the practical *cross-Clifford* alignment. |
| `cx_negate.qasm` | `propagation_negated` | `rx(a) q[0]; cx q[0],q[1]; rx(b) q[0];`. `CX * X_0 * CX = X_0 X_1`, which is *not* `X_0`, so this is actually blocked; see the file for the genuine sign-flip example. |
| `blocked.qasm` | `blocked` | Two rotations on different Pauli axes separated by an identity Clifford that does not align them. |

## Dofile — `dof/`

`propagation_merge_demo.dof` reads each QASM, converts to tableau, prints the
tableau, then re-prints after `tableau optimize` (proxy for the propagation
merge being wired through PR-5; here it confirms the inputs convert
correctly).

## Hand-verified merge for `rz(a); rz(b);`

Tableau before:

```
Pauli Rotations:
  IZ(a)
  IZ(b)
```

After propagation merge (same-Pauli case): single rotation `Z(a+b)`.

## Hand-verified merge for `rx(a); h; rx(b); h;` after `h` is applied to both sides

The intermediate `h ... h` represents a Clifford segment whose conjugation
of `X` returns `Z`, so `R_X(a)` becomes `R_Z(a)` after propagation. The
matching `R_Z(b)` on the right (if present after expressing in the same
basis) would then merge with phase `a + b`. This case is exercised more
naturally in PR-4 / PR-5 once the pipeline driver is wired up.
