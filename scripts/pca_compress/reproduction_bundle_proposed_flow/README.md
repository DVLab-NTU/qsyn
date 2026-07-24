# Reproduction Bundle — Proposed Flow (Heuristic)

Pipeline packaged here:

```
Hamiltonian
  → zero-sweep heuristic Pauli compression
  → Gridsynth (ε = 10⁻³) + Qsyn qzq QCO
  → Clifford+T
```

## `01_original_benchmarks/`

Each `<Bench>/` holds the **uncompressed** baseline before Pauli compression:

- `<Bench>.pauli` — Pauli list (canonical form)
- `<Bench>.qasm` — gate-level circuit **re-emitted from that Pauli list**

**LiH:** PySCF LiH (sto-3g, 6-orbital active space → 12 qubits) with
**Bravyi–Kitaev** mapping, \(\mathrm{d}t=0.05\) (same fermion-to-qubit convention
as the other molecular pickles in this suite). Not the older Hamlib `HF-BK12` stand-in.

How `.qasm` is produced: each list entry `(Pauli string, θ)` is expanded as one
Trotter term — Clifford conjugation to a local \(Z\), then `rz(θ)`, then uncompute —
and terms are concatenated in list order. The `.pauli` file is the source of truth;
the `.qasm` is the same list as OpenQASM 2.0. See also `emit_pauli_to_qasm.py`.

## `02_pauli_compressed/`

Compressed Pauli lists after the zero-sweep heuristic, grouped by fidelity tier
`F_gt_{0.999,0.99,0.9}` (selection uses \(F_{\mathrm{approx}} \ge F^\star\)):

- `<Bench>.pauli` — compressed list
- `<Bench>.qasm` — re-emitted from the compressed list (same procedure as above)

`#rot` here is the number of remaining **non-Clifford** Pauli rotations.

## `03_final_clifford_t/`

Final Clifford+T circuits after Gridsynth + qzq, same tier / benchmark layout:

- `<Bench>_clifford_t.qasm` — synthesized Clifford+T circuit
- `selection.txt` — short note for that point (method, \(F\), `#rot`, `#T`, …)

## `MANIFEST.csv`

Index of everything in this bundle: original rows plus one row per compressed/final point.
Columns include `stage`, `tier`, `benchmark`, `method`, `approx_fidelity`, `pauli_rot` (`#rot`), `qco_t` (`#T`), `qco_clifford`, and `note`.
Use this file for the metrics of the packaged circuits.

## `emit_pauli_to_qasm.py`

Standalone helper (stdlib only; no other repo files required) that converts a
`.pauli` list into OpenQASM 2.0 with the **same emit rules** used to build the
packaged `.qasm` under `01_` / `02_`. Use it to regenerate `.qasm` from any
Pauli list in this bundle (or to check that a packaged `.qasm` matches).

```bash
python3 emit_pauli_to_qasm.py 01_original_benchmarks/LiH/LiH.pauli
python3 emit_pauli_to_qasm.py 02_pauli_compressed/F_gt_0.999/LiH/LiH.pauli -o /tmp/LiH.qasm
python3 emit_pauli_to_qasm.py 01_original_benchmarks --check   # verify vs packaged .qasm
```
