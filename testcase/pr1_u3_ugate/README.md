# PR-1 — `U`/`U3` reader + ZYZ decomposition

The QASM 2.0 spelling variants `U`, `u`, `U3`, `u3` are all parsed into a
single `UGate(theta, phi, lambda)` operation whose canonical decomposition is

```
U(theta, phi, lambda) = RZ(phi) * RY(theta) * RZ(lambda)
```

(applied right-to-left; `lambda` is applied first to the state).

## QASM samples — `qasm/`

| File | Purpose |
|--|--|
| `u_clifford.qasm` | `u(pi/2, pi/2, pi/2)` and identity-equivalent calls. After `to_basic_gates`, the resulting circuit must be entirely Clifford. |
| `u_general.qasm` | `u(pi/3, pi/4, pi/5)`: arbitrary-angle smoke test for the parser. |
| `u_zyz_equiv.qasm` | A two-qubit circuit: qubit 0 holds `u(theta, phi, lambda)`; qubit 1 holds the manual `rz(lambda); ry(theta); rz(phi);`. Both single-qubit unitaries should be equal, so the full unitary is `U ⊗ U`. |
| `u2_basis.qasm` | `u2(phi, lambda) = U(pi/2, phi, lambda)`; confirms the 2-parameter spelling. |
| `u1_alias.qasm` | `u1(lambda)` should be parsed as `p(lambda)` (Qiskit alias). |

## Dofiles — `dof/`

| File | Purpose |
|--|--|
| `u3_read_print.dof` | Reads each QASM file and prints `qcir print --stat` to confirm gate counts. |
| `u3_to_tensor.dof` | Converts to tensor and prints, to check `to_tensor(UGate)` matches the QASM convention. |
| `u3_to_tableau.dof` | Converts to tableau and prints, to verify the ZYZ expansion lands as three sequential `RZ`/`RY`/`RZ` rotations. |
| `u3_to_basic.dof` | Runs `qcir to-basic` to confirm the UGate decomposes into three rotation gates. |

## Manual check

For `u(pi/3, pi/4, pi/5) q[0];`:

- `theta = pi/3`, `phi = pi/4`, `lambda = pi/5`.
- After `qcir to-basic`, the circuit is exactly:
  ```
  rz(pi/5) q[0];
  ry(pi/3) q[0];
  rz(pi/4) q[0];
  ```
- The corresponding tensor / tableau / ZX form must coincide with that
  three-gate sequence.

If a future change accidentally swaps the RZ order, `u_zyz_equiv.qasm`
will report unequal unitaries via `convert qcir tensor`.
