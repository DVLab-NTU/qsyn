# PR-9 `qcir cpf-optimize` end-to-end demo

Drives the full Continuous-Phase-Folding pipeline at the QCir IR level:

```
qcir cpf-optimize  ==  convert qcir tableau --trace-replay
                       tableau optimize cpf-global   (+ tmerge/hopt/phasepoly
                                                       unless --no-full)
                       convert tableau qcir
```

The intermediate Tableau is hidden from the user; this is the QCir-IR
counterpart of `tableau optimize cpf-full`.

## Files

| Path | Description |
| --- | --- |
| `qasm/cpf_chain.qasm`     | 2-qubit input with both an `RZ - H - RZ` ladder *and* an `RX - CX - RX` pair -- two distinct CPF wins in one circuit. |
| `qasm/clifford_t_mix.qasm`| 3-qubit Clifford-T mix with T gates and `RZ(pi/8)` injections.  Exercises the full pipeline on a non-trivial structural skeleton. |
| `dof/cpf_full_demo.dof`   | Reads each QASM, prints baseline statistics, runs `qcir cpf-optimize` (and the `--no-full` variant), then confirms `qcir equiv` against the original. |

## CLI summary

```
qcir cpf-optimize [--no-full] [-r/--replace]
```

* `--no-full`  -- skip the structural cleanup pass (`tmerge + hopt +
  ToddPhasePolynomialOptimizationStrategy`).  Useful for isolating
  CPF-only savings vs. the cleanup contribution.
* `--replace`  -- replace the focused QCir in place instead of adding a
  new managed copy.

## Notes / known cost

The CPF→Tableau→QCir round-trip uses the `HOpt` Clifford strategy and
the `naive` (CNOT-ladder) rotation strategy, the same defaults wired
into `tableau optimize cpf-full`.  For inputs with already-compact
Clifford structure the round-trip can *expand* the gate count even
though equivalence is preserved -- the trace-replay path is the safest
on the "equivalence" axis but not always optimal on "gate count".
This is the well-known cost of going through the Tableau IR and matches
what feynopt produces for the same inputs.
