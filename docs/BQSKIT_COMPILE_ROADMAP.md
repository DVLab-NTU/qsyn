# BQSKit `compile()` parity roadmap (CPF branch)

This document plans the remaining gap between qsyn `qcir to-u3cx` / `qcpfq` step-1
and BQSKit `bqskit.compile()`.

## Why 2-qubit demo KAK can fall back (not mainly “missing opt level”)

`testcase/pr10_pipeline/qasm/cpf_demo.qasm` is 2 qubits, 5 gates. Native `to-u3cx`
**used to** call `to_tensor(entire QCir)` → one `4×4` unitary → `kak_decompose`.

That failure mode is **numerical factorisation** of `K1,K2` after the magic-basis step
when the matrix comes from composing `RZ–H–RZ–CX–RZ`, not from a clean two-qubit
gate. BQSKit **does not** normally squash such a circuit into one SU(4) matrix;
it **partitions** into blocks (default `max_synthesis_size=3`) and runs
retarget / QSearch / LEAP + **instantiate** per block.

So:

| Cause | Relation to BQSKit |
|--------|-------------------|
| Monolithic `2^n` unitary synthesis | Architectural mismatch (fixed: multi-gate → partition) |
| Weak `factor_tensor_product` | qsyn KAK-only issue (improved; gray-code still OK) |
| Missing opt level 2–4 | Affects **gate count / polish after** structure exists |
| Missing QSearch/LEAP | Affects **per-block** synthesis quality, not KAK on whole circuit |

Equivalence is preserved via gray-code fallback; CNOT count may be worse than BQSKit.

## Is gate deletion standalone?

In BQSKit it is **`ScanningGateRemovalPass`** — a **pass inside** `compile()` workflows:

- Woven into `build_single_qudit_retarget_workflow` (single-qudit retarget)
- Woven into `build_multi_qudit_retarget_workflow` (multi-qudit case)
- **`build_gate_deletion_optimization_workflow`** at **optimization_level ≥ 2**
  (after mapping + retarget in `_opt2_workflow`)

It is not a separate top-level `bqskit.delete_gates()` API; users get it via
`compile(..., optimization_level=2+)`. QSD `FullQSDPass` can optionally append the
same pass between decomposition rounds.

qsyn counterpart (this branch): `compile_to_u3_cnot_impl(..., optimization_level≥2)`
runs a **lite** scanner in `circuit_compile.cpp` (remove a gate → `qfactor::instantiate`
to hold the pre-pass unitary).

## Phased implementation

| Phase | Scope | BQSKit reference | qsyn deliverable | Status |
|-------|--------|------------------|------------------|--------|
| **12a** | Multi-gate ≠ monolithic | `QuickPartitioner` + `ScanPartitioner` | `partition_circuit` (`Quick` / `Scan` / `QuickScan` default) + entangling split | **Done** |
| **12b** | `optimization_level` knob | `_opt1`…`_opt4` | CLI `--opt-level 1–3`; level 1 = `qfactor` polish | **Done (1–3)** |
| **12c** | Gate deletion | `ScanningGateRemovalPass` | `scanning_gate_removal_workflow` (U3 + CX, multi-pass) | **Done** |
| **13** | Block synthesis search | `QSearchSynthesisPass` / `LEAPSynthesisPass` | `--qsearch` / `--leap` → `scripts/bqskit_block_synth.py`; `--u3syn` full compile | **Done (needs bqskit)** |
| **13b** | QFAST / QPredict large blocks | optional passes | `--qfast` / `--qpredict` → `bqskit_block_synth.py` | **Done (needs bqskit)** |
| **14** | Resynthesis loop | `build_resynthesis_optimization_workflow` | `--opt-level 3` → `native_resynthesis` (lite) | **Done (lite)** |
| **14b** | QSD inter-round scan | `FullQSDPass` option | `QSDOptions::inter_round_gate_removal` in `tensor/qsd.cpp` | **Done** |
| **15** | Full opt4 / mapping | SABRE / SeqPAM | Out of scope (existing `device` / duostra) | N/A |

## CLI flags (naming)

| Flag | Meaning |
|------|---------|
| **`--u3syn`** | External BQSKit `compile()` via `scripts/bqskit_u3cx_compile.py` (partition + QSearch/LEAP + instantiate + opt). **Preferred** name for what was previously called “BQSKit path”. |
| **`--bqskit`** | Deprecated alias of `--u3syn`. |
| **`--qsearch`** | Per-block QSearch via `scripts/bqskit_block_synth.py` (requires `pip install bqskit`). |
| **`--leap`** | Per-block LEAP (same script). |
| **`--qfast`** | QFAST for blocks with ≥4 qubits (same script). |
| **`--qpredict`** | QPredict for large blocks (same script). |
| **`--opt-level`** | Native: 1=qfactor, 2=+U3 gate removal, 3=+resynthesis lite. Passed through to `--u3syn`. |

## CLI (after 12b–14)

```bash
qcir to-u3cx -r                      # native, opt 1, partitioned if >1 gate
qcir to-u3cx --opt-level 2 -r        # + scanning gate removal (U3)
qcir to-u3cx --opt-level 3 -r        # + resynthesis lite (re-partition)
qcir to-u3cx --monolithic -r         # force whole-unitary QSD (legacy)
qcir to-u3cx --u3syn --opt-level 2 -r   # full BQSKit compile()
qcir to-u3cx --bqskit -r             # same as --u3syn
```

## Remaining gaps vs BQSKit `compile()`

| Feature | BQSKit | CPF branch |
|---------|--------|------------|
| Native QSearch/LEAP (no Python) | in-process passes | Subprocess only (`--qsearch` / `--leap`) |
| ScanPartitioner (full) | exhaustive regions | `scan_partition` lite (MQ-gate scoring) |
| Topology-aware CX deletion | coupling graph | `--device-topology` + `topology_align_cx` (opt≥2) |
| opt 4 / SeqPAM | `_opt4` | Use qsyn `device` / duostra |
| AutoRebase / multi-pass retarget | retarget workflows | `retarget_to_u3_cx` only |
| Strict CNOT optimality | search + LEAP | Native: KAK vs gray-code pick; use `--qsearch` for parity |
