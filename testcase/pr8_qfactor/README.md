# PR-8 QFactor-lite (`qcir instantiate`) demo

Numerical instantiation of a fixed-topology ansatz against a target
unitary, using coordinate descent on every UGate parameter
(theta, phi, lambda).  Non-UGate gates (`CX`, `H`, ...) are held fixed,
matching the QFactor philosophy of "trust the topology, polish the
leaves".

## Files

| Path | Description |
| --- | --- |
| `qasm/u3_random.qasm`                  | Single `U(0.7, 1.3, 2.1)` -- the target. |
| `qasm/u3_perturbed.qasm`               | Same topology, angles shifted by ~0.2 rad. |
| `qasm/two_qubit_ansatz_target.qasm`    | 2-qubit ansatz with six U3 gates and two CX. |
| `qasm/two_qubit_ansatz_perturbed.qasm` | Same gate sequence, every U3 angle perturbed by 0.1 ~ 0.2 rad. |
| `dof/qfactor_demo.dof`                 | Reads each (target, perturbed) pair, runs `qcir instantiate --target <id>` on the perturbed circuit and confirms equivalence afterwards. |

## Expected output

After each `qcir instantiate ...` call the two `qcir equiv` lines should
flip from *not equivalent* to *equivalent*, confirming that QFactor-lite
recovered the target angles to within the tolerance.

## CLI summary

```
qcir instantiate [--target ID | --self]
                 [--max-iter N] [--tol T] [--init-step S] [-v]
```

* `--target ID` -- compute the tensor of QCir `ID` and use that as the
  optimisation goal.  Defaults to comparing the focused circuit against
  its own tensor (i.e. residual = 0 going in -- useful as a no-op
  regression).
* `--max-iter`  -- hard cap on coordinate-descent passes (default 200).
* `--tol`       -- residual threshold (`1 - cosine_similarity`) at which
  the run terminates early.
* `--init-step` -- initial probe step in radians; halved on stagnation.

The command only ever mutates UGate parameters; structural gates pass
through untouched.
