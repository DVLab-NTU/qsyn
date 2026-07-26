# qsyn: Supported Synthesis and Optimization Algorithms

This document lists the synthesis and optimization algorithms currently supported in qsyn, with brief descriptions and **limitations** that may matter when implementing new algorithms (e.g. **trasyn**, **Non-Clifford Fusion**, **equivalence checking with non-π/4 rotations**).

---

## 1. Synthesis Algorithms

### 1.1 Tableau → Circuit (Tableau to QCir)

**Command:** `convert tableau qcir` with options `--clifford` and `--rotation`.

#### Clifford (stabilizer) synthesis

| Strategy | CLI | Description |
|----------|-----|-------------|
| **HOpt** | `--clifford hopt` (default) | Optimal Hadamard count for Clifford+T synthesis of Pauli rotation sequences. Diagonalizes stabilizers with provably optimal Hadamards, then Aaronson–Gottesman. Ref: [Vandaele et al., arXiv:2302.07040](https://arxiv.org/abs/2302.07040). |
| **AG** | `--clifford ag` | Aaronson–Gottesman extraction. Ref: [Improved simulation of stabilizer circuits](https://journals.aps.org/pra/abstract/10.1103/PhysRevA.70.052328); see also Qiskit `clifford_decompose_ag`. |
| **HStair** | `--clifford hstair` | HOpt in staircase mode. |

#### Pauli-rotation synthesis

| Strategy | CLI | Description |
|----------|-----|-------------|
| **Naive** | `--rotation naive` (default) | Each Pauli rotation is conjugated to a single-qubit Z rotation: [C†][RZ][C]. Simple, no shared structure. |
| **GraySynth** | `--rotation graysynth` | Gray-code-based synthesis (star mode). |
| **GStair** | `--rotation gstair` | GraySynth in staircase mode. |
| **Mst** | `--rotation mst` | Minimum spanning arborescence for Pauli rotation ordering. Ref: [Vandaele et al., arXiv:2104.00934](https://arxiv.org/abs/2104.00934). |
| **TPar** | `--rotation tpar` | **Not implemented.** Logs "TPar Synthesis Strategy is not implemented yet!!" and returns failure. |

**Limitation (rotation synthesis):** All rotation strategies emit **RZ** gates for non-Clifford phases. The tableau stores the angle in **exp(i θ P)**; conversion assumes this is used as the RZ angle. **TPar** is a placeholder for future work (e.g. T-parallelism / trasyn-style algorithms).

---

### 1.2 Tensor → Circuit

| Algorithm | Command | Description |
|-----------|---------|-------------|
| **Solovay–Kitaev** | `sk-decompose -d <depth> -r <recursion>` | Approximate single-qubit (or small) unitary decomposition into a gate set (e.g. Clifford+T) with Solovay–Kitaev. |
| **Direct decomposition** | (used internally, e.g. after convert) | ZYZ decomposition, Gray-code-style synthesis, and multi-controlled gate decomposition (CnU, etc.) in `src/tensor/decomposer.hpp`. |

**Note:** Tensor→QCir is used for numeric unitaries; it does not use the tableau/phase-polynomial pipeline.

---

### 1.3 ZX-based flow

Circuit can be converted to ZX (`convert qcir zx`), then ZX simplification and extraction back to circuit. The ZX simplifier has its own rules (see §2.3); extraction back to QCir is separate from tableau synthesis.

---

## 2. Optimization Algorithms

### 2.1 Tableau optimization

**Command:** `tableau optimize <method> [options]`.

| Method | Description | Limitation |
|--------|-------------|------------|
| **full** | Iterate: merge rotations → minimize internal Hadamards → **phase polynomial (TODD)** until T-count stops decreasing. | **TODD requires every phase to be a 4th root of unity** (i.e. denominator of phase divides 4). If any Pauli rotation has a non-π/4 multiple (e.g. Hamiltonian or NCF angles), TODD logs: *"Failed to perform TODD optimization: the polynomial contains a non-4th-root-of-unity phase!!"* and skips that block. **full** is intended for Clifford+T (T = π/4) circuits. |
| **collapse** | Collapse tableau to a canonical form. | No phase restriction. |
| **tmerge** | Merge rotations that share the same rotation plane (same Pauli). | No phase restriction. |
| **hopt** | Minimize Hadamard count and internal Hadamards. | No phase restriction. |
| **phasepoly** | Phase polynomial optimization (TODD). Strategy: `todd`. | **Same as full: phases must be 4th roots of unity.** |
| **ncf** | **Non-Clifford Fusion (NCF).** Partition Pauli rotations into groups that conjugate to 1 (or 2) qubits; replace by blocks [C†][R'][C]. Ref: [arXiv:2510.13573](https://arxiv.org/abs/2510.13573). | Works with **arbitrary phases**. Intended for reducing T-count/depth and for compatibility with arbitrary-angle circuits (e.g. Hamiltonian simulation). |
| **equiv** | Lightweight optimization for equivalence checking: **tmerge + hopt only**, no phase polynomial / TODD. | **Safe for arbitrary phases.** Use this when comparing circuits with non-π/4 rotations (e.g. before/after NCF or Hamiltonian). |
| **matpar** | Matroid partition: partition Pauli rotations into simultaneously implementable tableaux (with optional ancillae). Strategy: `naive`. | **Requires all Pauli rotations to be diagonal** (phase polynomial form). |

**Summary for implementers:**

- **Arbitrary phases (non-π/4):** Use **ncf**, **equiv**, **tmerge**, **hopt**, **collapse**. Do **not** use **full** or **phasepoly** (TODD) on such tableaux.
- **T-count reduction for Clifford+T:** Use **full** (or **tmerge** + **hopt** + **phasepoly** manually).
- **New algorithms (e.g. trasyn):** Can be added as new tableau optimization methods or new rotation synthesis strategies (e.g. implementing **TPar** or a new strategy that exploits T-parallelism).

---

### 2.2 QCir optimization

**Command:** `qcir optimize [strategy] [options]`.

| Strategy | Description |
|----------|-------------|
| **basic** | Gate-level optimization: fuse phases, cancel CNOTs/CZs, Hadamard exchange, CRZ transform, optional swap, CZ↔CX conversion. Config: `--physical`, `--tech`, `--statistics`, `--copy`. |
| **teleport** | Phase teleport. |
| **blaqsmith** | Two-qubit count optimization (annealing-style), `optimize_2q_count`. |

When `--tech` is set or a gate set is specified, **trivial** (tech) optimization is used instead of basic (no swap, gate-set preserving).

**Limitation:** These operate on QCir only; they do not use the tableau or phase polynomial. For circuits with arbitrary phases, basic/trivial/teleport/blaqsmith do not impose π/4.

---

### 2.3 ZX simplification

**Command:** `zx optimize [mode]` or `zx rule <rule>`.

**Modes:** `--full` (default), `--dynamic`, `--symbolic`, `--partition`, `--interior-clifford`, `--clifford`, `--causal`.

**Rules:** bialgebra, gadget-fusion, hadamard-fusion, hadamard-rule, identity-removal, local-complementation, pivot, pivot-boundary, pivot-gadget, spider-fusion, state-copy, to-z-graph, to-x-graph, redundant-hadamard-insertion.

ZX simplification is independent of the tableau phase-polynomial (TODD) restriction; it works on ZX graphs with arbitrary phases.

---

## 3. Equivalence Checking

### 3.1 QCir equivalence (`qcir equiv <id1> <id2>`)

1. Compose adjoint(circuit1) with circuit2 and convert to **tableau**.
2. Run **optimize_for_equiv** on that tableau (tmerge + hopt only; **no TODD**).
3. If the optimized tableau is empty → equivalent.
4. Else, if qubit count ≤ 7: convert optimized tableau → QCir → **tensor**, then compare to identity with `tensor::is_equivalent(..., 1e-5)`.
5. If qubits > 7, tensor fallback is skipped and the command may report "not equivalent" (possible false negative).

**Limitation:**  
- **Arbitrary phases are supported** because TODD is not used (optimize_for_equiv).  
- **Tensor fallback is limited to ≤ 7 qubits** and can be expensive.  
- Equivalence is **not** proven by TODD-based normalization when phases are non-π/4; the proof is either “tableau simplifies to empty” or “circuit tensor equals identity”.

### 3.2 Tableau equivalence (`tableau equiv <id1> <id2>`)

1. Convert each tableau → QCir (HOpt + NaivePauliRotations).
2. Convert each QCir → tensor.
3. Compare the two tensors with `tensor::is_equivalent(..., 1e-5)`.

**Limitation:**  
- Only supported for **≤ 7 qubits** (same as QCir tensor fallback).  
- Uses **NaivePauliRotations** for both; no TODD. Safe for **arbitrary phases**.

---

## 4. Where to Extend for New Algorithms

| Goal | Where in qsyn | Current limitation / note |
|------|----------------|---------------------------|
| **trasyn** (T-parallelism synthesis) | New **PauliRotationsSynthesisStrategy** (e.g. extend or replace **TPar** in `tableau_to_qcir`) and/or new tableau optimization. | TPar is a stub; no T-parallelism yet. |
| **Non-Clifford Fusion** | Already implemented: `tableau optimize ncf`. | Can be extended (e.g. different grouping, 2-qubit blocks). |
| **Equivalence with non-π/4 rotations** | **qcir equiv** and **tableau equiv** already avoid TODD: they use **optimize_for_equiv** and/or direct tensor comparison. | Tensor comparison limited to 7 qubits; no TODD-based canonical form for arbitrary phases. |
| **Phase polynomial optimization for arbitrary phases** | **TODD** in `src/tableau/optimize/todd.cpp` explicitly rejects non-4th-root phases. | New method would need a different representation (e.g. symbolic or numeric) and new rewrite rules. |
| **Matroid partition with non-diagonal rotations** | **matpar** requires diagonal (phase polynomial) rotations. | Generalizing to non-diagonal would require new independence oracle / partition strategy. |

---

## 5. Quick Reference: Phase and TODD

- **Phase in tableau:** Stored as angle θ in **exp(i θ P)** (rational multiple of π).
- **TODD:** Only applies when **4 % rotation.phase().denominator() == 0** (4th roots of unity). Check in `src/tableau/optimize/todd.cpp`.
- **Safe for arbitrary phases:**  
  - **optimize_for_equiv**, **ncf**, **tmerge**, **hopt**, **collapse**;  
  - **qcir equiv** (uses optimize_for_equiv + tensor);  
  - **tableau equiv** (tensor comparison only).  
- **Not safe for arbitrary phases:**  
  - **full_optimize** (includes TODD), **phasepoly** (TODD).

This file can be updated as new algorithms are added or limitations are relaxed.
