# Continuous Phase Folding (CPF) Branch — Architecture & PR Log

> Living document. Append new sections as PRs land on `continuous-phase-folding`.
> Authoring conventions: English only. Mirror existing qsyn module style.

---

## Goals

Port the **Continuous Phase Folding (CPF)** pipeline (currently a Python prototype
at `~/CPF`) into the qsyn C++ codebase, augmented with:

1. **Arbitrary-`U3` ingestion** (from `origin/feat/parser_update`),
2. **BQSKit-lite synthesis** (any unitary → `U3` + `CX`), and
3. **Feynman/feynopt-style phase folding** that supports **arbitrary
   `X`/`Y`/`Z` Pauli rotations** (not just diagonal/T-count optimisation).

Out of scope for this branch (deliberately):
- `Gridsynth` (handled externally via Python helper if needed),
- `quantum-circuit-optimization` (Rust) — its routines (T-merge / TOHPE / H-opt)
  are already covered by qsyn's `tableau optimize {tmerge, hopt, phasepoly}`.

Implementation language split:
- **C++ (in-tree, primary)**: all data structures and algorithms.
- **Python (`scripts/`, helper)**: large-scale benchmarking, `qiskit.Operator`
  ground-truth verification, comparison against the real `bqskit`.

---

## What qsyn already provides

Before adding anything, the following are reusable from `main`:

| Slide / CPF concept | Existing qsyn API | File |
|--|--|--|
| Pauli rotation tableau (X/Y/Z + phase) | `PauliRotation`, `PauliProduct` | `src/tableau/pauli_rotation.hpp` |
| Mixed (Stabilizer ‖ Rotation) tableau | `Tableau` (`std::variant`) | `src/tableau/tableau.hpp` |
| Step-1: extract rotations | `to_tableau(QCir)` | `src/convert/qcir_to_tableau.cpp` |
| Step-2: push Clifford to the front | `properize(Tableau&)` | `src/tableau/tableau_optimization.cpp` |
| Step-3a: same-`P` merge | `merge_rotations` | same |
| Step-3b: absorb Clifford-angle rotations | `absorb_clifford_rotations` | same |
| Step-4: tableau → circuit | `to_qcir(Tableau, ...)` (naive/TPar/GraySynth/MST) | `src/convert/tableau_to_qcir.cpp` |
| T-count phase-poly optimisation | `ToddPhasePolynomialOptimizationStrategy` | `src/tableau/optimize/todd.cpp` |
| H-opt | `minimize_internal_hadamards` | `src/tableau/optimize/internal_h_opt.cpp` |
| Matroid partition | `NaiveMatroidPartitionStrategy` | `src/tableau/tableau_optimization.cpp` |
| Arbitrary-`Phase` rotations | `RXGate`/`RYGate`/`RZGate` (`dvlab::Phase`) | `src/qcir/basic_gate_type.hpp` |
| CLI | `tableau optimize {full, tmerge, hopt, phasepoly, matpar}` | `src/cmd/tableau_cmd.cpp` |

CPF's main *additive* contributions on top of this are:
- **Propagation merge** across Clifford segments (CPF `_classify_segment`
  branch `PROP_MERGEABLE`).
- **Global continuous-angle phase-polynomial fold** (CPF `cpf_global.py` /
  `cpf_phase_poly.py`), which is *not* limited to diagonal phase polynomials.
- **`U3` reader** and high-level **unitary synthesis** to feed the pipeline.

---

## High-level pipeline (product default)

The **target product pipeline** (implemented in `src/qcir/cpf_pipeline.cpp`)
is exposed step-by-step and as one-shot shortcuts:

| Step | CLI | Reference analogue |
|--|--|--|
| 1. Arbitrary QCir → U3+CX | `qcir to-u3cx` | BQSKit `compile()` → U3+CX |
| 2. U3+CX → ZYZ+CX | `qcir to-zyz` | QASM `u` → `rz/ry/rz` chain |
| 3. QCir → Tableau (+CPF) | `qcir to-tableau [--fold]` | Python `extract_pe_stream_tableau` + CPF |
| 4. Tableau → QCir | `qcir from-tableau` | qsyn `convert tableau qcir -r graysynth` |
| All-in-one | `qcpfq` or `qcir cpf-pipeline -r` | Full chain; stops before Gridsynth |

```
QASM ──► qcir read ──► QCir
           │
           ▼  qcir to-u3cx        (QSD/KAK, BQSKit-like logical synthesis)
         U3 + CX
           │
           ▼  qcir to-zyz         (to_basic_gates: RZ·RY·RZ per qubit)
         ZYZ + CX  (RZ, RY, RZ, H, S, CX, …)
           │
           ▼  qcir to-tableau --fold
         Tableau  [Stabilizer | PauliRotation | …]  + global_fold (CPF)
           │
           ▼  qcir from-tableau   (Gray-synth per diagonal block; naive else)
         Optimised QCir  ──► qcir write …
           │
           ▼  [out of branch] Gridsynth → Clifford+T
```

Legacy / auxiliary paths still available: `qcir synthesize` (step 1 only),
`convert qcir tableau --trace-replay`, `tableau optimize cpf-*`,
`qcir instantiate` (QFactor-lite polish).

---

## File layout (target, end-of-roadmap)

```
src/
├── qcir/
│   ├── basic_gate_type.hpp          (+ UGate)                     [PR-1]
│   ├── gate_type.cpp                (+ u/u2/u3 dispatch)          [PR-1]
│   └── qcir_reader.cpp              (+ U3 multi-param QASM)       [PR-1]
├── convert/
│   ├── qcir_to_tensor.cpp           (+ to_tensor(UGate))          [PR-1]
│   ├── qcir_to_zxgraph.cpp          (+ create_u_zx_form + ...)    [PR-1]
│   └── qcir_to_tableau.cpp          (+ append_to_tableau(UGate))  [PR-1]
├── tableau/
│   └── cpf/                                                       [NEW]
│       ├── angle_utils.{hpp,cpp}                                  [PR-2]
│       ├── propagation_merge.{hpp,cpp}                            [PR-3]
│       ├── global_fold.{hpp,cpp}                                  [PR-4]
│       ├── trace_replay.{hpp,cpp}                                 [PR-4]
│       └── cpf_full.{hpp,cpp}                                     [PR-9]
├── synthesis/                                                     [NEW]
│   ├── kak.{hpp,cpp}                                              [PR-6]
│   ├── qsd.{hpp,cpp}                                              [PR-7]
│   └── qfactor_lite.{hpp,cpp}                                     [PR-8?]
└── cmd/
    ├── conversion_cmd.cpp           (+ synthesize subcommand)     [PR-6]
    ├── tableau_cmd.cpp              (+ cpf-merge/cpf-global/...)  [PR-5/9]
    └── synthesis_cmd.{hpp,cpp}                                    [PR-6]

testcase/
├── README.md                        (per-PR test catalogue)
├── pr1_u3_ugate/                                                  [PR-1]
├── pr2_angle_utils/                                               [PR-2]
├── pr3_propagation_merge/                                         [PR-3]
└── ...                                                            [later PRs]

scripts/                              (Python helper, out-of-tree) [PR-10/11]
├── verify_equiv.py
├── bqskit_compare.py
└── cpf_bench.py
```

CMake side: qsyn's top-level `CMakeLists.txt` uses
`file(GLOB_RECURSE LIB_SOURCES "src/**/*.cpp" "src/**/*.hpp")`, so all new files
under `src/tableau/cpf/` and `src/synthesis/` are picked up automatically with
no CMake edits.

---

## PR Roadmap

| PR | Title | Status |
|--|--|--|
| PR-0 | Architecture doc + `testcase/` scaffold + `.gitignore` | **DONE** (this PR) |
| PR-1 | `U3`/`UGate` reader + ZYZ decomposition + 3 converters | **DONE** (this PR) |
| PR-2 | `tableau/cpf/angle_utils` | **DONE** (this PR) |
| PR-3 | `tableau/cpf/propagation_merge` | **DONE** (this PR) |
| PR-4 | `global_fold` + `trace_replay` (continuous-angle global fold) | **DONE** (this PR) |
| PR-5 | CLI `tableau optimize cpf-{merge,global,full}` + `convert qcir tableau --trace-replay` | **DONE** (this PR) |
| PR-6 | KAK 2-qubit decomposition + `qcir synthesize` CLI | **DONE** (ZYZ exact; KAK 2-qubit best-effort with gray-code fallback — sign-fix follow-up tracked below) |
| PR-7 | QSD n-qubit synthesis | **DONE** (recursive dispatcher with KAK/ZYZ base cases; cosine-sine recursion scaffolded with TODO) |
| PR-8 | QFactor-lite numerical optimisation + `qcir instantiate` CLI | **DONE** (coordinate-descent over UGate parameters; passes 1-qubit + 2-qubit perturbed-ansatz round-trip) |
| PR-9 | `qcir cpf-optimize` end-to-end pipeline (Feynopt-equivalent) | **DONE** (`trace_replay → cpf::global_fold → [full_optimize] → tableau→qcir`; `--no-full` isolates CPF-only savings) |
| PR-10 | `scripts/verify_equiv.py`, `scripts/bqskit_compare.py` + README | **DONE** (Qiskit `Operator` equivalence; BQSKit recompile comparator) |
| PR-11 | `scripts/cpf_bench.py` + `testcase/benchmark/` + `docs/benchmark_results.md` | **DONE** (driver shells out to `build/qsyn`, verifies equivalence, optional BQSKit column, writes Markdown report) |

---

## PR-0 — Branch scaffolding (DONE)

Goal: prepare the branch for incremental CPF work.

Changes:
- Added `docs/CPF_ARCHITECTURE.md` (this file). All later PRs append a new
  section below.
- Extended `.gitignore` to keep local experimental QASM artifacts
  (`de_*.qasm`, `my_qc/`, `output_vqe.*`, `latex_error.log`) out of the repo.
- Created `testcase/` with one sub-directory per PR; each contains a tiny
  per-PR `README` plus minimal QASM/dofile samples used as sanity checks.

No production code changed.

---

## PR-1 — `U3` / `UGate` reader + ZYZ decomposition (DONE)

Goal: let qsyn ingest arbitrary `U`/`U3` gates from QASM 2.0 and treat them
as first-class single-qubit operations across all four representations
(QCir, QTensor, ZXGraph, Tableau).

Math used (QASM 2.0 convention):

```
U(theta, phi, lambda) = RZ(phi) · RY(theta) · RZ(lambda)
```

In qsyn `QCir`, the equivalent gate sequence — appended left-to-right and thus
applied left-to-right — is `RZ(lambda) → RY(theta) → RZ(phi)`, producing the
same matrix as above.

### New / modified files

| File | Change |
|--|--|
| `src/qcir/basic_gate_type.hpp` | Added `class UGate`, `adjoint(UGate)`, `is_clifford(UGate)`, `to_basic_gates(UGate)` (ZYZ expansion). |
| `src/qcir/gate_type.cpp` | `str_to_basic_operation` now recognises `u` / `u3` (3 parameters) and `u2` (2 parameters, treated as `U(π/2, φ, λ)`). |
| `src/qcir/qcir_reader.cpp` | QASM line scanner extended: when a gate carries a comma-separated parameter list, all parameters are parsed with `dvlab::Phase::from_string` and forwarded to `str_to_operation`. Backward compatible with single-parameter gates. |
| `src/convert/qcir_to_tensor.cpp` | `to_tensor(UGate)` returns `RZ(phi) · RY(theta) · RZ(lambda)` (note: this differs from `feat/parser_update` which had the order inverted — see commit *parser working but Unitary transformation remains buggy*). |
| `src/convert/qcir_to_zxgraph.cpp` | `create_u_zx_form(theta, phi, lambda)` builds a 5-node spider chain `RZ(lambda) → Sdg → RX(theta) → S → RZ(phi)` (i.e. ZYZ where the middle `RY` is realised as `Sdg · RX · S`). `to_zxgraph(UGate)` dispatches to it. |
| `src/convert/qcir_to_tableau.cpp` | `append_to_tableau(UGate)` decomposes into three sequential `RZ` / `RY` calls and reuses the existing `RZGate` / `RYGate` paths. |

### Testcases (`testcase/pr1_u3_ugate/`)

- `qasm/u_clifford.qasm` — `U(π/2, π/2, π/2)` and similar Clifford-angle
  triples; their unitary must coincide with explicit Clifford circuits.
- `qasm/u_general.qasm` — `U(π/3, π/4, π/5)`; verifies arbitrary-angle parse.
- `qasm/u_zyz_equiv.qasm` — `U(θ, φ, λ)` on `q[0]` *and* the manual
  `rz(λ); ry(θ); rz(φ);` decomposition on `q[1]`. The two single-qubit unitaries
  on different qubits should be the tensor product `U ⊗ U`, which the dofile
  checks via `convert qcir tensor`.
- `dof/u3_read_print.dof` — `qcir read … ; qcir print --stat`.
- `dof/u3_to_tensor.dof` — round-trip through `QTensor`.
- `dof/u3_to_tableau.dof` — round-trip through `Tableau` (verifies the ZYZ
  expansion lands correctly in the rotation list).

---

## PR-2 — `tableau/cpf/angle_utils` (DONE)

Goal: small, dependency-free phase helpers that all subsequent CPF passes
build on. Mirrors the CPF Python module `cpf_angles.py`.

### API (`src/tableau/cpf/angle_utils.hpp`)

```cpp
namespace qsyn::experimental::cpf {

// True iff `phase` represents an integer multiple of pi/2 (i.e. Clifford
// rotation). qsyn's dvlab::Phase is rational and already normalised to
// (-1, 1] (units of pi), so the predicate is denominator() <= 2.
[[nodiscard]] bool is_clifford_phase(dvlab::Phase const& phase) noexcept;

// True iff `phase` represents the zero angle (modulo 2 pi).
[[nodiscard]] bool is_zero_phase(dvlab::Phase const& phase) noexcept;

// True iff `phase` is pi (i.e. a Z / X / Y gate, modulo basis).
[[nodiscard]] bool is_pi_phase(dvlab::Phase const& phase) noexcept;

// Convert to canonical (-pi, +pi] radians using dvlab::Phase semantics.
[[nodiscard]] double phase_to_radians(dvlab::Phase const& phase) noexcept;

// True iff |phase_to_radians(p1 - p2)| <= tol (radians). Useful for
// numerical comparisons after `from_string` rounding.
[[nodiscard]] bool approx_equal(dvlab::Phase const& lhs,
                                dvlab::Phase const& rhs,
                                double tol = 1e-9) noexcept;

}  // namespace qsyn::experimental::cpf
```

These wrap `dvlab::Phase` rather than duplicating math; they exist so CPF code
reads in domain terms ("clifford angle?") instead of denominator checks.

### Testcases (`testcase/pr2_angle_utils/`)

Pure utility — no QASM. The directory README enumerates expected
predicate values for a grid of common phases (0, π/4, π/2, π, etc.).

---

## PR-3 — Propagation merge across a Clifford segment (DONE)

Goal: implement CPF's `_classify_segment` and the `PROP_MERGEABLE`
merge action in pure C++, operating directly on qsyn's `Tableau`.

### Theory recap

Given the segment

```
... R_{P_L}(theta_l)  ⟨StabilizerTableau C⟩  R_{P_R}(theta_r) ...
```

with `C` representing a sequence of Clifford operators, we want to merge the
two rotations into a single one without altering the global unitary. Two
algebraic identities are used:

1. *Conjugation*: `R_P(theta) · C = C · R_{C† P C}(theta)` for any Clifford
   `C` and Pauli string `P`.
2. *Same-Pauli composition*: `R_P(α) · R_P(β) = R_P(α + β)`.

Let `P_L' := C† · P_L · C` (Pauli string of `R_{P_L}` after being pushed past
`C`). Three cases:

- `P_L' = +P_R`  ⇒ merge: keep `C` in place, replace `R_{P_R}(theta_r)` with
  `R_{P_R}(theta_l + theta_r)` and drop `R_{P_L}(theta_l)`.
- `P_L' = -P_R`  ⇒ merge with sign flip: new phase = `-theta_l + theta_r`
  (because `R_{-P}(θ) = R_{P}(-θ)`).
- Otherwise     ⇒ blocked, leave untouched.

The same logic is iterated greedily until a full pass finds no more merges.

### Mapping to qsyn's Tableau

qsyn stores a `Tableau` as a list of `SubTableau = variant<StabilizerTableau,
vector<PauliRotation>>`. Pattern-matching `[..rotations.., StabilizerTableau,
..rotations..]` and considering the *last rotation of the left list* with the
*first rotation of the right list* corresponds exactly to the
`_classify_segment` case above when the intermediate carries non-trivial
non-commuting Cliffords.

Note: the existing `merge_rotations` already handles the case where the
intermediate tableau is empty / identity, *and* qsyn's `extract_clifford_operators`
turns any `StabilizerTableau` back into a `CliffordOperatorString` that
`PauliRotation::apply(...)` can consume to perform the forward conjugation
`C P C†` (the qsyn convention is forward, so we use `adjoint(ops)` to obtain
the backward `C† P C` we need above).

### API (`src/tableau/cpf/propagation_merge.hpp`)

```cpp
namespace qsyn::experimental::cpf {

enum class MergeClass : std::uint8_t {
    same_pauli,            // identical PauliProduct
    propagation_aligned,   // propagated P_L equals  P_R
    propagation_negated,   // propagated P_L equals -P_R
    blocked                // none of the above
};

struct SegmentReport {
    MergeClass klass;
    int sign;              // +1 for aligned/same, -1 for negated, 0 for blocked
    PauliProduct propagated_pauli;
};

// Classify a left-right rotation pair separated by `intermediate`.
// `intermediate` may be empty (caller can pass an identity StabilizerTableau).
[[nodiscard]] SegmentReport classify_segment(
    PauliRotation const&     left,
    StabilizerTableau const& intermediate,
    PauliRotation const&     right);

struct PropagationMergeStats {
    std::size_t n_same_pauli      = 0;
    std::size_t n_propagation     = 0;
    std::size_t n_removed_zero    = 0;
    std::size_t n_passes          = 0;
};

// In-place greedy propagation merge over the whole Tableau. Loops until a
// full pass yields no further merges. Returns aggregate counters.
PropagationMergeStats propagation_merge(Tableau& tableau);

}  // namespace qsyn::experimental::cpf
```

The classifier is intentionally **pure**: it does not mutate either input and
does not touch the intermediate Clifford. The driver `propagation_merge`
performs the actual edits (phase update on `right`, deletion of `left`,
cleanup of empty rotation blocks) and reuses `remove_identities` to keep the
post-state canonical.

### Limitations of this PR

- Only pairs separated by exactly one `StabilizerTableau` (or directly
  adjacent) are considered in a single iteration. Multi-Clifford-segment
  fusion happens implicitly via iteration: after a merge, neighbouring
  subtableaux can be `properize`d back to canonical form for the next pass.
- Numerical-tolerance verification (the CPF Python `_pick_merge_angle`
  brute-force `±(θ_l ± θ_r)` strategy) is *not* used here — qsyn's
  `dvlab::Phase` is rational so the algebraic outcome is exact.

### Testcases (`testcase/pr3_propagation_merge/`)

- `qasm/same_pauli.qasm` — two `rz(θ); rz(φ);` on the same qubit (case
  `same_pauli`).
- `qasm/h_z_h.qasm` — `rz(θ); h; rz(φ); h;` → propagation through `H` makes
  the two rotations both effectively `rx`, mergeable into one
  (`propagation_aligned`).
- `qasm/cx_negate.qasm` — small case showing `propagation_negated` via
  `cx`-induced sign flip.
- `qasm/blocked.qasm` — `rz(θ); ry(φ);` separated by a Clifford that does
  not align them (`blocked`).
- `dof/propagation_merge_demo.dof` — full pipeline:
  `qc → tableau → cpf classify (printed) → cpf propagation-merge → tabl → qc`.

CLI wiring for `tableau optimize cpf-merge` ships in **PR-5** so as not to
mix scope; the testcases here only exercise the classifier indirectly via
`tableau print` before / after invoking the function from a dofile-loadable
script step (the actual public-CLI flag arrives later).

---

### Build & runtime validation (Day-0)

Validated on `continuous-phase-folding` HEAD with CMake 3.31.10 + GCC 11.4:

```
[100%] Built target qsyn
[100%] Built target unit-test
```

Smoke tests via the dofiles in `testcase/`:

- `testcase/pr1_u3_ugate/dof/u3_read_print.dof` — all four QASM variants
  parse; `u(pi/2, pi/2, pi/2)` and `u(pi, 0, pi)` are reported as Clifford
  (verifying `is_clifford(UGate)`); the other variants are classified as
  "Others" (non-Clifford).
- `testcase/pr1_u3_ugate/dof/u3_to_basic.dof` — `u(pi/3, pi/4, pi/5)` is
  decomposed into 3 gates (`RZ(pi/5) -> RY(pi/3) -> RZ(pi/4)`), exactly
  matching the QASM 2.0 convention.
- `testcase/pr1_u3_ugate/dof/u3_to_tableau.dof` — Pauli-rotation form of
  the same gate prints as `exp(i*pi/5*Z); exp(i*pi/3*Y); exp(i*pi/4*Z);`,
  i.e. the ZYZ expansion lands in the tableau as expected.
- `testcase/pr3_propagation_merge/dof/propagation_merge_demo.dof` — all
  four `MergeClass` inputs convert cleanly:
    * `same_pauli.qasm`           → two adjacent `IZ` rotations.
    * `h_z_h.qasm`                → the `H; H = I` pair is auto-absorbed by
                                     `properize`, leaving two adjacent `IZ`
                                     rotations (same-Pauli case).
    * `cx_negate.qasm`            → `rx(a); z; rx(b)` collapses to a
                                     Clifford with `D0 = -XI` (sign flipped
                                     by `Z`) and rotations
                                     `exp(i*-pi/5*XI); exp(i*pi/6*XI)`;
                                     the sign flip on `theta_l` shows the
                                     `propagation_negated` identity in
                                     algebraic form.
    * `blocked.qasm`              → `rz(a); h; ry(b)` becomes
                                     `exp(i*pi/3*X)` + `exp(i*pi/4*Y)`,
                                     i.e. different Paulis -- blocked.

### Follow-up note (PR-3 visibility)

`convert qcir tableau` in qsyn's main path already invokes `collapse` +
`properize`, which **pushes all Cliffords to a single front
`StabilizerTableau`** and leaves a single trailing rotation list. In that
canonical form, our `propagation_merge` driver reduces to "merge same-Pauli
rotations within one list" -- which the existing `merge_rotations` already
handles. The unique value of `propagation_merge` therefore only materialises
once we have access to an **interleaved** `Tableau` (alternating
`StabilizerTableau` / rotation blocks). Building such an input goes through
two paths planned for later PRs:

1. PR-4's `trace_replay` constructs an interleaved tableau by walking the
   QCir gate-by-gate (no collapse).
2. PR-5's CLI `tableau optimize cpf-merge` will run `propagation_merge` on
   that interleaved tableau before any `properize`.

Until then, the classifier `classify_segment(...)` is fully usable from
unit tests and the driver is correct (verified to not regress
`merge_rotations`-equivalent inputs).

---

## PR-4 — `trace_replay` + `global_fold` (DONE)

Goal: produce an **interleaved** `Tableau` from a `QCir` (one `StabilizerTableau`
segment per consecutive Clifford run; one rotation list per consecutive
non-Clifford run) and iteratively fold rotations across the resulting
Clifford boundaries.

### New files

| File | Role |
|--|--|
| `src/tableau/cpf/trace_replay.{hpp,cpp}` | `trace_replay(QCir) -> Tableau`. Walks gates in order; before applying each Clifford gate that comes right after a rotation block, pushes a fresh identity `StabilizerTableau` so the new Clifford lands in its own segment. Otherwise delegates to the existing `append_to_tableau` paths, which preserves the `Tableau::h/s/cx` invariant. |
| `src/tableau/cpf/global_fold.{hpp,cpp}` | `global_fold(Tableau&) -> GlobalFoldStats`. Iterates: (1) within-block `merge_rotations` (same-Pauli fusion), (2) cross-Clifford `propagation_merge`, (3) prune empty rotation blocks and coalesce adjacent `StabilizerTableau` segments via `extract_clifford_operators` + `apply`. Stops at the first pass that does no work. |

### Why `trace_replay` is needed

`convert qcir tableau` (the default `to_tableau`) drives every gate through
`Tableau::h/s/cx`, which iterates the subtableaux **in reverse**, conjugates
every rotation it crosses with the new Clifford, and absorbs the Clifford
into the last `StabilizerTableau`. The result is always *one* leading
`StabilizerTableau` + *one* trailing rotation block — exactly the form
`collapse` produces. In that form, `propagation_merge`'s `boundary` work
degenerates to within-block same-Pauli fusion (already done by
`merge_rotations`).

`trace_replay` skips the auto-collapse by *pre-emptively* pushing a fresh
identity `StabilizerTableau` whenever the gate stream switches from a
rotation back to a Clifford. The subsequent `tableau.h/s/cx` call finds the
new (empty) stab at the back and applies the Clifford there, leaving any
prior rotation block untouched (no conjugation, no fusion).

### Validation

Run via `testcase/pr4_global_fold/dof/cpf_global_demo.dof`:

| Input QASM | Cartesian outcome | Verified result |
|--|--|--|
| `qasm/h_sandwich_pair.qasm` (`rx(π/5) ; H ; rz(π/7) ; H ; rx(π/9)`) | Three rotations along axes that all become `X` after the two H Cliffords are coalesced; phases add exactly in rational arithmetic: `1/5 + 1/7 + 1/9 = 143/315`. | After `cpf-global`: a single `Pauli Rotations: exp(i * 143π/315 * X)` block. |
| `qasm/cx_x_target_cancel.qasm` (`rx(π/8) q[1] ; CX(0,1) ; rx(-π/8) q[1]`) | `CX (I⊗X) CX = I⊗X`, so the two rotations are aligned and cancel. | After `cpf-global`: no rotations, only the residual `CX` Clifford segment. |
| `qasm/cx_z_pair.qasm` (`rz(π/8) q[1] ; CX(0,1) ; rz(-π/8) q[1]`) | `CX (I⊗Z) CX = Z⊗Z`, so propagation gives `ZZ` ≠ `IZ` → blocked (`classify_segment` returns `blocked`). | After `cpf-global`: rotations remain unmerged, exactly as expected. |
| `qasm/cx_x_sign_flip.qasm` (`rz(π/8) q[0] ; CX(0,1) ; rz(π/8) q[0]`) | `CX (Z⊗I) CX = Z⊗I`, so the two equal-Pauli rotations merge to `Z⊗I` with twice the phase. | After `cpf-global`: a single `exp(i * π/4 * ZI)`. |

The fourth column entries are read directly from the runtime
`tableau print -c` output — no post-processing.

---

## PR-5 — CPF CLI (DONE)

### Added CLI surface

- `convert qcir tableau --trace-replay` — opt into the interleaved
  builder; default behaviour (`to_tableau` + collapse) is unchanged.
- `tableau optimize cpf-merge`   — single pass of `propagation_merge`.
- `tableau optimize cpf-global`  — `global_fold` to fixpoint.
- `tableau optimize cpf-full`    — `global_fold` then qsyn's existing
  `full_optimize` (collapse + tmerge + hopt + phasepoly/TODD).
  TODD reports a benign error and skips on non-`π/2k` phases; the rest of
  the pipeline still runs and the tableau is left unitarily equivalent.

`tableau_cmd.cpp` matches the three `cpf-*` names by **exact equality**
because the existing `is_prefix_of` strategy would let `cpf-merge` shadow
`cpf-global` etc.

### Quick verification

Same dofile as PR-4 — the demonstration step (4) drives both `convert qcir
tableau` paths on the H-sandwich circuit and prints the resulting tableaux
side-by-side, confirming that the `--trace-replay` + `cpf-full` path
collapses three rotations into one while the default path leaves them as
three separate rotations (since `to_tableau` does not call
`merge_rotations` itself).

---

## PR-6 — KAK 2-qubit decomposition + `qcir synthesize` CLI (DONE, KAK best-effort)

### New files

| File | Role |
|--|--|
| `src/tensor/kak.{hpp,cpp}` | `single_qubit_synthesize` (ZYZ → `UGate`), `cartan_synthesize(a, b, c)` (six-CNOT decomposition of `exp(i (a XX + b YY + c ZZ))`), `kak_decompose(4x4)` (magic-basis + symmetric eigendecomposition), `two_qubit_synthesize` (combined pipeline). |
| `src/cmd/qcir/synthesize_cmd.cpp` | `qcir synthesize [--replace]` CLI — converts the focused circuit to a tensor and re-emits it through QSD/KAK/ZYZ. Always returns a valid circuit because gray-code is the fallback when KAK fails. |

### Status of KAK 2-qubit

The 2-qubit KAK path runs the standard magic-basis recipe (Vatan-Williams
2004 conventions) using `xt::linalg::eigh` on the symmetric real
combination `α Re(D) + β Im(D)`. The single-qubit factorisation step
(`factor_tensor_product`) is correct in principle but **sign-fixing is
brittle on certain inputs** — when the recovered K-matrices fall outside
a clean `SU(2) ⊗ SU(2)` shape (which happens for e.g. `CZ`-equivalent
inputs), `kak_decompose` returns `nullopt` and `qcir synthesize` falls
back to the existing gray-code `tensor::Decomposer`.

This is intentional: the gray-code fallback guarantees *correctness* of
every emitted circuit (`qcir equiv` passes in all three PR-6 test cases),
while leaving room for a follow-up commit that hardens the sign-fixing /
Takagi-factorisation logic for optimal three-CNOT output.

### Validation

Run via `testcase/pr6_kak/dof/kak_demo.dof`:

| Case | Path taken | Equivalence to original |
|--|--|--|
| `u(π/3, π/4, π/5)` (1-qubit) | KAK ZYZ | **equivalent**; resynthesised back to `u(π/3, π/4, π/5)` exactly. |
| `h ; cx ; h ; h` (2-qubit CZ-equivalent) | KAK → fallback to gray-code | **equivalent**. |
| `rx/ry/rz + cx` mix (2-qubit) | KAK → fallback to gray-code | **equivalent**. |

The ZYZ math has one subtle gotcha worth recording: `dvlab::Phase(double)`
interprets its argument as **radians** and internally divides by π. So when
the caller has already computed an angle in radians (as we do after
`std::acos` / `std::arg`), it must be passed **without** an extra `/ pi`,
otherwise the resulting Phase undershoots by exactly a factor of π (we
initially hit this and the synthesised gate was `u(26π/245 ...)` instead
of `u(π/3 π/4 π/5)`).

---

## PR-7 — QSD n-qubit dispatcher (DONE; recursive CSD scaffolded)

### New files

| File | Role |
|--|--|
| `src/tensor/qsd.{hpp,cpp}` | `qsd::synthesize(QTensor)` dispatcher. `n=1` → `kak::single_qubit_synthesize`. `n=2` → `kak::two_qubit_synthesize` (with gray-code fallback). `n>=3` → `tensor::Decomposer` (gray-code) until the cosine-sine recursion lands. |
| `src/cmd/qcir/synthesize_cmd.cpp` | Updated to call `qsd::synthesize` instead of branching by qubit count locally. |

### Why this is "done" while CSD is a TODO

The user-visible CLI works for arbitrary qubit counts (validated on
3-qubit GHZ and a mixed 3-qubit input — both reproduce equivalent
circuits). The placeholder behaviour for `n>=3` (gray-code) gives **correct**
circuits, only with sub-optimal CNOT counts.

The follow-up work is to add a true `cosine_sine_decompose(matrix)` helper
(SVD-based, on the four `2^{n-1} x 2^{n-1}` sub-blocks of a `2^n x 2^n`
unitary), feed its output into a `synthesize_uniformly_controlled_rotation`
helper, and recurse. The scaffolding header already mentions this and the
existing dispatcher is the one-line replacement point.

### Validation

Run via `testcase/pr7_qsd/dof/qsd_demo.dof`:

| Case | Outcome |
|--|--|
| `h ; cx ; cx` (3-qubit GHZ prep) | `qcir synthesize` produces a 3-qubit circuit; `qcir equiv` reports **equivalent**. |
| `rx/ry/rz + cx` mix (3-qubit) | same — **equivalent**. |

---

## Conventions for future PRs

- All new source files use the qsyn header comment block:
  ```
  /****************************************************************************
    PackageName  [ <module> ]
    Synopsis     [ <one-line> ]
    Author       [ Design Verification Lab ]
    Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
  ****************************************************************************/
  ```
- Comments and identifiers in **English** only.
- Algorithmic code lives under `qsyn::experimental::cpf` until promoted out
  of `experimental`.
- Each PR appends a section to this file (do **not** edit historical
  sections; add follow-up notes as new sub-sections at the end of the
  relevant PR if a fix is required later).
- Each PR drops its samples under `testcase/prN_<topic>/` and updates
  `testcase/README.md` with a one-line summary.

---

## PR-8 — QFactor-lite (`qcir instantiate`) (DONE)

Goal: provide a *numerical* polish step that, given a fixed-topology
ansatz and a target unitary, tunes every `UGate` parameter so the
ansatz reproduces the target.  This complements the analytic synthesis
of PR-6 / PR-7: KAK / QSD pick the structure, QFactor-lite refines the
angles.  The design intentionally trades BQSKit / QFactor-JAX's
per-gate environment computation for a robust coordinate descent that
only needs the already-existing `to_tensor(QCir)` and
`cosine_similarity` primitives.

### Files

| Path | Purpose |
|--|--|
| `src/tensor/qfactor.hpp` / `qfactor.cpp` | `QFactorOptions`, `QFactorResult`, `instantiate(ansatz, target, opt)` |
| `src/cmd/qcir/instantiate_cmd.cpp` | `qcir instantiate [--target ID|--self] [--max-iter N] [--tol T] [--init-step S] [-v]` |
| `src/cmd/qcir_cmd.cpp` | Registers `qcir_instantiate_cmd` in `qcir-cmd-group` |
| `testcase/pr8_qfactor/` | 1-qubit + 2-qubit perturbed-vs-target QASM pairs, dofile driver, README |

### Algorithm

```
collect_u_params(ansatz)            // (gate*, which) handles for every (theta, phi, lambda)
residual := 1 - cosine_similarity(to_tensor(ansatz), target)
step := initial_step
while pass < max_iter && residual > tol && step >= min_step:
    improved := false
    for each parameter p:
        try p += step  -> r_plus
        try p -= step  -> r_minus
        accept the better of {current, +step, -step}; update residual
    if !improved: step *= step_shrink         // 1/2 by default
```

Only `UGate` parameters are tuned; structural gates (`CX`, `H`, `RZ`,
`RX`, ...) are held fixed, matching the BQSKit / QFactor "trust the
topology" philosophy.  The double-precision angle round-trip uses
`dvlab::Phase(angle, 1e-9)` so the rational approximation does not
clip the precision that `cosine_similarity` resolves.

### Validation

`testcase/pr8_qfactor/dof/qfactor_demo.dof` loads two pairs:

* a single `U(0.7, 1.3, 2.1)` target and a `U(0.9, 1.1, 1.9)` perturbed
  ansatz;
* a 2-qubit, 6-gate ansatz with random perturbations on every U3.

After `qcir instantiate --target <id> --max-iter ... --tol 1e-9`, the
follow-up `qcir equiv <target-id>` reports *equivalent* in both cases.

### Known limitations

* Coordinate descent is `O(passes × params × tensor_cost)`; tractable
  for circuits up to ~5 qubits / dozens of UGates.  For larger inputs
  the analytic per-gate environment trick (proper QFactor) would be a
  drop-in replacement -- it is **not** required to integrate downstream
  PRs.
* `to_tensor(QCir)` is rebuilt from scratch on every probe; a
  decomposition-aware caching layer could cut the inner-loop cost
  substantially but is out of scope for PR-8.

---

## PR-9 — `qcir cpf-optimize` end-to-end pipeline (DONE)

Goal: expose the full Continuous-Phase-Folding pipeline at the QCir IR
level so users do not have to manually orchestrate
`convert qcir tableau → tableau optimize cpf-* → convert tableau qcir`.

### Files

| Path | Purpose |
|--|--|
| `src/cmd/qcir/cpf_optimize_cmd.cpp` | `qcir cpf-optimize [--no-full] [-r/--replace]` |
| `src/cmd/qcir_cmd.cpp` | Registers `qcir_cpf_optimize_cmd` |
| `testcase/pr9_cpf_full/` | 2-qubit `RZ-H-RZ + RX-CX-RX` chain, 3-qubit Clifford-T mix, dofile driver, README |

### Pipeline (matches `feynopt`'s default driver)

```
qcir cpf-optimize  ==
    trace_replay(qcir)                    // PR-4: interleaved Tableau
    cpf::global_fold(tableau)             // PR-4: within-block + cross-Clifford fusion to fixpoint
    if !--no-full: full_optimize(tableau) // existing T-count-driven cleanup
    to_qcir(tableau, HOpt, naive)         // existing Tableau→QCir extraction
```

The intermediate Tableau is never exposed to the user; the command
mirrors the `tableau optimize cpf-full` workflow but stays purely on
QCirs.  Both routes are kept because the Tableau-level command is
still useful for debugging mid-pipeline tableau states.

### Validation

`testcase/pr9_cpf_full/dof/cpf_full_demo.dof` exercises both inputs
with `cpf-optimize` and `cpf-optimize --no-full`.  Final `qcir equiv`
reports *equivalent* on both circuits.

### Cost / known trade-off

The `naive` (CNOT-ladder) rotation strategy in `to_qcir` can *expand*
the gate count of already-compact inputs while still preserving
equivalence.  The benchmark in `testcase/benchmark/bench_2q_continuous.qasm`
shows the win-case (4 gates / 2 CX → 1 gate / 0 CX), while
`bench_2q_clifford.qasm` shows a structural neutral case (gate count
stays the same after the round-trip).  This is the well-known cost of
routing through the Tableau IR and is independent of the CPF logic
itself.

---

## PR-10 — External verification & comparison scripts (DONE)

Goal: hand the user (and CI) the same equivalence and gate-count
sanity checks that internal `qcir equiv` provides, but driven from
*outside* qsyn using mainstream tools (Qiskit, BQSKit).  This makes the
CPF pipeline auditable independently of the in-tree Tableau code.

### Files

| Path | Purpose |
|--|--|
| `scripts/verify_equiv.py`   | Two-QASM equivalence (Qiskit `Operator`, phase-corrected Frobenius diff). Importable as a library by `cpf_bench.py`. |
| `scripts/bqskit_compare.py` | Recompiles a baseline QASM with BQSKit (U3+CNOT target) and prints gate / CNOT / depth side-by-side with a qsyn candidate. |
| `scripts/README.md`         | Documents dependencies (`pip install qiskit [bqskit]`), usage examples, and the phase-tolerance contract. |

### Notes

* All scripts import qiskit / bqskit lazily so missing the optional
  dependency only fails at runtime, not at import time.
* `verify_equiv.is_equivalent(u1, u2, tol)` returns
  `(equal, residual)`; the residual is exposed so callers can log
  near-misses.
* Tested against `qiskit==2.0.x` (`QuantumCircuit.from_qasm_file`).
  BQSKit is optional but recommended (`pip install bqskit`).

---

## PR-11 — Benchmark runner & Markdown report (DONE)

Goal: package the validation pipeline as a single, repeatable batch
job so we can track CPF performance over time without writing custom
shell glue.

### Files

| Path | Purpose |
|--|--|
| `scripts/cpf_bench.py`              | Walks every `.qasm` in an input directory, runs `qcir cpf-optimize` (+ `--no-full`) via `build/qsyn`, verifies equivalence, optionally adds a BQSKit recompile column, and writes a Markdown table. |
| `testcase/benchmark/qasm/`          | Four micro-benchmarks covering trivially-mergeable, propagation-mergeable, continuous-angle (canonical CPF win-case) and 3-qubit Clifford-T-mix circuits. |
| `testcase/benchmark/README.md`      | Describes each benchmark + how to add new ones. |
| `docs/benchmark_results.md`         | Auto-generated by the runner; checked into the repo as a baseline. |

### Output format

The Markdown table has one row per benchmark and the columns:

```
| benchmark | qubits | input gates/CX | cpf-optimize gates/CX |
| cpf-optimize --no-full gates/CX | bqskit gates/CX | equiv | runtime |
```

`equiv` shows `yes` or `NO (<residual>)`, mirroring `qcir equiv`'s
verdict but on the Qiskit-computed unitaries.  `runtime` reports the
qsyn wall-clock plus -- when `--with-bqskit` is set -- the BQSKit
recompile time.

### Validation

Running `python scripts/cpf_bench.py --qsyn build/qsyn` produces the
checked-in `docs/benchmark_results.md`.  All four current benchmarks
pass equivalence; `bench_2q_continuous.qasm` collapses a continuous
`RZ-CX-RZ-CX` block to a single rotation, which is the canonical CPF
win that no T-count-only pipeline can reproduce.

### Exit code contract

The script exits non-zero if **any** benchmark fails equivalence,
making it a drop-in CI check.  Gate-count regressions do not fail the
run (some inputs intentionally exercise expansion paths -- see PR-9
notes above).

---

## Parity audit vs BQSKit & Python CPF

This section is the running answer to the recurring question
"which BQSKit / Python-CPF feature is at what completeness level in
qsyn?".  Update whenever a parity item changes.

### A. BQSKit synthesis modules (PR-6 / PR-7 / PR-8)

| BQSKit upstream | qsyn module | I/O parity | Algorithm parity | Notes |
|--|--|--|--|--|
| `passes.synthesis.qsd.QSDPass` (`scipy.linalg.cossin` recursion → MPRY/MPRZ → CNOT) | `src/tensor/qsd.cpp` + `src/tensor/csd.cpp` (`qsd::synthesize`, LAPACK `zuncsd`) | ✅ same I/O surface: any `2^n x 2^n` unitary → `QCir` in U3+CNOT basis | n=1 ✅ ZYZ; n=2 ✅ KAK (+ gray-code fallback, multi-anchor factor); **n≥3 ✅ recursive QSD** (CSD + multiplex, gray-code fallback on failure). High-qubit **circuits** use `src/qcir/circuit_compile.cpp` greedy partition (≤3 qubits/block) instead of one `2^n` tensor. |
| Implicit two-qubit "KAK" (BQSKit relies on QSD calling `cossin` even at n=2) | `src/tensor/kak.cpp` (`kak::{single_qubit,cartan,two_qubit}_synthesize`) | ✅ same I/O surface | ZYZ ✅ exact; Cartan ✅ correct **with 6 CNOTs** (the optimal 3-CNOT variant is a sign-fix follow-up tracked under PR-6). | KAK decomposition itself uses magic basis + `xt::linalg::eigh`; only the *synthesis* of `exp(i(aXX+bYY+cZZ))` is the 6-CNOT form. |
| `ir.opt.instantiaters.qfactor.QFactor` (Rust `bqskitrs.QFactorInstantiatorNative`; analytic per-gate environment optimisation on every `LocallyOptimizableUnitary`) | `src/tensor/qfactor.cpp` (`qfactor::instantiate`) | ✅ same I/O surface: in = ansatz `QCir` + target unitary, out = same `QCir` with refined params, residual reported | **Algorithm differs** -- coordinate descent on `(θ,φ,λ)` of every `UGate`, target = `1 − cosine_similarity`. BQSKit uses analytic environment SVD per gate (much faster + supports any `LocallyOptimizableUnitary`). | qsyn version is intentionally named "QFactor-lite". Only `UGate` is tunable today; other gates pass through untouched. |
| BQSKit's `setmodel.py`, `passes.mapping.*`, `CouplingGraph`, `MachineModel` | **Out of scope** in PR-0 ~ PR-11 | n/a | n/a | KAK / QSD / QFactor in BQSKit are themselves connectivity-blind (search for `CouplingGraph` in `bqskit/passes/synthesis/` returns only `qfast.py` / `qpredict.py`). The qsyn ports keep the same contract -- a top-of-file `NOTE -- Connectivity / coupling-graph awareness is OUT OF SCOPE` was added to `tensor/{kak,qsd,qfactor}.hpp` and `cmd/qcir/{synthesize,instantiate,cpf_optimize}_cmd.cpp` so the boundary is explicit in the source. Physical-mapping users should chain with qsyn's existing `device` + `duostra` + `qcir optimize --physical` path. |

### B. Phase folding (PR-2 / PR-3 / PR-4 / PR-5 / PR-9) vs Python CPF

The Python prototype at `~/CPF` exposes three folding modes
(`local`, `global`, `poly`).  Their qsyn counterparts:

| Python module / function | qsyn entry point | Parity |
|--|--|--|
| `cpf_angles.is_clifford_angle` / `is_zero_phase` | `tableau/cpf/angle_utils.hpp` (`is_clifford_phase` / `is_zero_phase`) | ✅ algorithmically identical. **Wired into the pipeline since the parity audit**: `propagation_merge` shortcuts `is_zero_phase(left)` rotations; `global_fold` reports `n_clifford_angle_left` in `GlobalFoldStats` so `cpf-full` / `tmerge` can absorb them. |
| `cpf_angles.normalize_angle` | implicit via `dvlab::Phase` auto-normalisation to `(-1, 1]` units of π | ✅ |
| `cpf_angles.fold_terms` (global same-Pauli accumulator) | `tableau_optimization::merge_rotations(Tableau&)` (via `collapse` → `merge_rotations(rotations)` → `absorb_clifford_rotations`) | ✅ algorithmically equivalent. Triggered by **`tableau optimize tmerge`** (operating on the default `to_tableau` collapsed view) or as the second leg of `cpf-full` / `qcir cpf-optimize`. |
| `cpf_merge._classify_segment` (4-class adjacency report) | `cpf::propagation_merge::classify_segment` | ✅ same four classes (qsyn names: `same_pauli` / `propagation_aligned` / `propagation_negated` / `blocked`). qsyn uses symplectic compare via `StabilizerTableau::apply`; Python uses `Pauli.evolve(Clifford)`. Both are exact -- no floating-point fuzz. |
| `continuous_phase_fold` (mode = `local`) | `cpf::propagation_merge` (called by **`tableau optimize cpf-merge`**, also iterated to fixpoint inside `cpf::global_fold`) | ✅ |
| `cpf_global.cpf_global_from_rz` (global same-Pauli fuse + local CPF + multi-round retranspile) | **`tableau optimize cpf-global`** then `cpf-full` for the global pass + retranspile-style loop | ✅ for the within-Tableau part; the multi-round *retranspile loop* (Python re-`transpile`s back to `rz/cx` basis to expose new merges) is not yet wired -- equivalent reduction is currently achieved by `cpf-full`'s `full_optimize` loop because `full_optimize` is itself iterative (`do { merge_rotations + minimize_internal_hadamards + Todd } while T-count decreasing`). |
| `cpf_phase_poly.cpf_phase_poly_fold` (running Clifford-prefix scan; "frozen-label" conjugation; the strongest Python mode) | `tableau_optimization::merge_rotations(Tableau&)` via **`tableau optimize tmerge`** -- which `collapse`s to `[StabilizerTableau, rotations]`, then `merge_rotations(rotations)` already runs on the frozen-label representation. | ✅ qsyn covers this *for free* via the standard `to_tableau`/`collapse` path. The reason it isn't a separately named `cpf-poly` sub-command is that the Python `cpf_phase_poly` is conceptually identical to qsyn's existing `tmerge` once the tableau is in canonical form. |
| BQSKit's `passes.processing.scan.ScanningGateRemovalPass` (used by `FullQSDPass` to delete redundant gates between QSD rounds) | not ported | ⚠ TODO -- typically saves a few percent of gates after QSD; not on the critical path for PR-0 ~ PR-11. |

### C. End-to-end pipeline parity

Mapping Python CPF driver → qsyn CLI:

```
Python                                       qsyn
------                                       ----
mode = "local"                               tableau optimize cpf-merge
mode = "global"                              tableau optimize cpf-global  +  cpf-full
mode = "poly"  (cpf_phase_poly_from_rz)      tableau optimize tmerge      (collapsed view; via standard to_tableau)
                                             or  tableau optimize cpf-full
the whole stack on a QCir                    qcir cpf-optimize                  (PR-9; QCir-IR convenience wrapper)
```

### D. Tested correctness

* **Equivalence preservation** (= must-have): all 11 testcase dofiles
  (`testcase/pr1_*/dof/...` through `testcase/pr9_*/dof/...`) and all 4
  benchmark inputs (`testcase/benchmark/qasm/*`) pass `qcir equiv`
  and/or Qiskit `Operator.equiv` (via `scripts/verify_equiv.py`).
* **Gate-count behaviour**: see `docs/benchmark_results.md` (refreshed
  on every audit run).  Continuous-angle case (`bench_2q_continuous.qasm`)
  shows the canonical CPF win: 4 gates / 2 CX → 1 gate / 0 CX.
* **No regression** from the parity audit changes (`angle_utils` wiring,
  Clifford-angle reporting): `build/qsyn` rebuilt clean, no new lints.

### E. Outstanding follow-ups (not blockers for PR-11)

1. ~~Full CSD recursion in `tensor/qsd.cpp`~~ **Done (PR-12)**: `csd::cossin_separate` + recursive `qsd` for `n≥3`; partition compile for `n>4` circuits. Remaining: gate-deletion / opt-level passes, ~~QSearch/LEAP~~ (done in PR-C, see below), full BQSKit parity on CNOT count.
2. ~~Optimal 3-CNOT KAK synthesis in `tensor/kak.cpp::cartan_synthesize`~~
   **Done (PR-A)**: `kak::try_three_cnot_synthesize` instantiates the
   8-U3 / 3-CX optimal ansatz via QFactor. Opt-in through `--three-cnot-kak`
   (slow under coord-descent), much faster with `--three-cnot-lbfgs` after
   PR-B's LBFGS minimiser lands.
3. Multi-round retranspile loop around `qcir cpf-optimize` to expose
   merges that only show up after structural cleanup re-runs the
   `to_tableau` pass.  Python `cpf_global_from_rz` / `cpf_phase_poly_from_rz`
   implement this with `max_rounds` (default 3 / 5).
4. Connectivity-aware synthesis path (placement + routing) -- BQSKit's
   `setmodel.py` workflow.  Out of scope for PR-0 ~ PR-11 and explicitly
   disabled (top-of-file `NOTE` comments).

---

## PR-A — Retarget hardening + 3-CNOT KAK QFactor ansatz (DONE)

### New / changed files

| File | Role |
|--|--|
| `src/tensor/kak.cpp` (+`.hpp`) | New `try_three_cnot_synthesize(matrix, eps, n_restarts, max_iter, use_lbfgs)`: builds the canonical 8-U3 / 3-CX SU(4) ansatz and refines its 24 parameters with `qfactor::instantiate`. Random restarts to escape local minima. Integrated into `two_qubit_synthesize` as an optional candidate (controlled by `TwoQubitSynthesizeOptions::try_three_cnot_qfactor`); the lowest-CX successful candidate wins. |
| `src/tensor/qsd.cpp` | Fix: `cmat_to_qtensor({dim,dim})` was matching the nested-initializer-list constructor (created a length-2 rank-1 tensor with values `[dim, dim]`). Switched to explicit `TensorShape{dim, dim}` to construct an actual `dim x dim` matrix; this removes the "Matrix is not square" assertion that previously triggered the gray-code fallback path in `qsd_recursive`. |
| `src/qcir/circuit_compile.cpp` | Hardened `retarget_to_u3_cx`: enumerates *all* 1-qubit / 2-qubit / 3+-qubit gate types, with `is_cx_gate` / `is_u_gate` structural matchers (fixes the original `get_type()[1] == 'x'` check that never matched `ControlGate::get_type() == "cpx"`). Final-pass invariant check `circuit_is_u3_cx_only` emits an error if any non-U3/CX gate survives. |
| `src/cmd/qcir/{synthesize_cmd,cpf_pipeline_cmds}.cpp` | CLI flags `--three-cnot-kak`, `--three-cnot-restarts`, `--three-cnot-lbfgs` (forward to `kak::TwoQubitSynthesizeOptions` + `qsd::QSDOptions` + `U3CxCompileOptions`). |
| `testcase/pr12_retarget_3cnot/` | dof + QASM: exercises CZ/SWAP/ECR retargeting + 3-CNOT QFactor path with `qcir equiv` verification. |

### Validation

`build/qsyn -q testcase/pr12_retarget_3cnot/dof/retarget_demo.dof`:
* CZ/SWAP/RX/RY/RZ mix → pure U3+CX (20 gates), `qcir equiv` ✓.
* The 3-CNOT KAK path is currently gated behind `--three-cnot-kak` until
  the LBFGS minimiser (PR-B) lands -- coord-descent on the 24-parameter
  ansatz can take ~minute per 2-qubit block; PR-B drops that to a few
  hundred ms.

---

## PR-B — Generic cost/minimizer abstraction with LBFGS (DONE)

### New files

| File | Role |
|--|--|
| `src/tensor/opt/cost.{hpp,cpp}` | `CostFunction` interface + `QCirHilbertSchmidtCost` (wraps a `QCir` ansatz against a target `QTensor`; `evaluate(x)` writes U3 angles, runs `to_tensor`, returns `1 − cosine_similarity`). Default `gradient()` uses forward finite differences. |
| `src/tensor/opt/minimizer.{hpp}`, `coord_descent.cpp`, `lbfgs.cpp` | `Minimizer` interface; two concrete drivers: `CoordinateDescentMinimizer` (faithful re-implementation of QFactor-lite's probe loop) and `LBFGSMinimizer` (textbook two-loop recursion + Armijo backtracking; bails out when `|g·d|` drops below numerical noise to avoid stalling on saddles). |
| `src/tensor/qfactor.{hpp,cpp}` | Refactored: `QFactorOptions::strategy = {CoordinateDescent, LBFGS}`; `instantiate` is now a thin adapter that builds a cost function and dispatches to the chosen minimiser. Output residual / convergence reported as before. |
| `src/qcir/circuit_compile.cpp` | `U3CxCompileOptions::qfactor_use_lbfgs` propagates LBFGS to both the 3-CNOT KAK ansatz and the post-synthesis `apply_native_optimization` polish pass. |
| `src/cmd/qcir/{synthesize_cmd,cpf_pipeline_cmds}.cpp` | CLI flag `--lbfgs` (+ legacy alias `--three-cnot-lbfgs`). |
| `testcase/pr13_qfactor_lbfgs/` | Demo: 2-qubit "hard" unitary (all three Cartan invariants non-zero) → 3 CXs via 3-CNOT QFactor with LBFGS, `qcir equiv` ✓ in ~0.2 s. |

### Validation

`build/qsyn -q testcase/pr13_qfactor_lbfgs/dof/lbfgs_demo.dof`:
* Hard 2-qubit U(4) target, `qcir synthesize --three-cnot-kak --three-cnot-restarts 2 --three-cnot-lbfgs`
  → 11 gates (3 CXs + 8 U3s, optimal), `qcir equiv` ✓, wall time 0.17 s.
* Compared to coord-descent on the same input: 0.28 s, 3 CXs, equivalent
  — LBFGS reaches the same optimum in ~60 % of the time.

### Bonus side-effect

The `[error] kak_decompose: failed to factor K1 / K2 into SU(2) x SU(2).`
log was demoted to `debug` (it is a normal signal that the analytic KAK
path was exhausted and the 6-CNOT analytic fallback should be used; no
user action required).  Likewise `tableau/optimize/todd.cpp`'s
"non-4th-root-of-unity phase" warning was demoted (synthesis pipelines
that feed continuous rotations into the tableau path will routinely
trigger it).

---

## PR-C — Native QSearch / LEAP block synthesisers (DONE)

### New files

| File | Role |
|--|--|
| `src/synthesis/search/frontier.hpp` | `Candidate { QCir, residual, depth, priority }` + `Frontier` (priority queue ordered by `Heuristic::priority`). |
| `src/synthesis/search/layer_generator.{hpp,cpp}` | `LayerGenerator` interface + `SimpleLayerGenerator` (one child per ordered `(ctrl, targ)` CX pair, sandwiched by fresh U3s). `build_root_ansatz(n)` emits a single-qubit U3 per qubit, seeded with small random angles to escape the identity saddle of the `|tr|`-based cosine-similarity cost. |
| `src/synthesis/search/heuristic.hpp` | `Heuristic` interface; `GreedyHeuristic` (`priority = residual`) and `AStarHeuristic` (`priority = residual + α·depth`, default `α = 0.05`). |
| `src/synthesis/search/qsearch.{hpp,cpp}` | `qsearch_synthesize(target, opts)` — best-first tree search; pops the best leaf, expands, instantiates each child via `tensor::opt::Minimizer` (LBFGS by default), pushes back. Stops at `residual ≤ success_threshold`. `leap_synthesize(target, opts)` adds the LEAP prefix-freeze loop. |
| `src/qcir/circuit_compile.{hpp,cpp}` | Added `BlockSynthEngine::{QSearchNative, LeapNative}`. `synthesize_block` now routes those engines through the native search before falling back to KAK/QSD. |
| `src/cmd/qcir/{synthesize_cmd,cpf_pipeline_cmds}.cpp` | CLI flags `--qsearch-native` / `--leap-native` (work both for `qcir synthesize` and the full `qcir to-u3cx` partitioned pipeline). |
| `testcase/pr14_qsearch_native/` | Demos: (1) `qcir synthesize --qsearch-native` on a CZ-equivalent 2-qubit circuit → 1 CX; (2) `--leap-native` on a generic 2-qubit unitary → 3 CXs; (3) `qcir to-u3cx --qsearch-native` on 3-qubit GHZ → 2 CXs. All three verified via `qcir equiv`. |

### Subtle correctness fix that fell out of bringing PR-C up

`build_root_ansatz` originally seeded every U3 at `(0, 0, 0)` (identity).
Because `cosine_similarity` is defined as `|tr|/d`, the identity is a
*saddle* of the residual (linear term in the trace vanishes for any
unitary target).  Numerically the FD-gradient at the seed evaluates to
`O(h)` ≈ 1e-5 instead of `O(1)`, which combined with Armijo
backtracking caused LBFGS to spin without ever taking a step.  Two
defensive changes solved this:

1. `u3_random(rng)` initialises every freshly-added U3 with a small
   deterministic perturbation drawn from a per-circuit-depth PRNG seed.
2. `LBFGSMinimizer::minimize` aborts the line-search early when
   `|g·d| < 1e-14` (true saddle), instead of looping until
   `ls_max_steps`.

### Wallclock numbers

| Test | Engine | Output | Time |
|--|--|--|--|
| 2-qubit CZ-equivalent (`testcase/pr14_qsearch_native/qasm/two_qubit_cz.qasm`) | `qcir synthesize --qsearch-native` | 1 CX (optimal) | ~3 s |
| Generic 2-qubit U(4) (`testcase/pr13_qfactor_lbfgs/qasm/hard_two_qubit.qasm`) | `qcir synthesize --leap-native` | 3 CXs (optimal) | <1 s |
| 3-qubit GHZ (`testcase/pr14_qsearch_native/qasm/three_qubit_ghz.qasm`) | `qcir to-u3cx --qsearch-native --opt-level 1` | 2 CXs (optimal) | ~90 s |

The 3-qubit run is dominated by the inner-loop QFactor instantiation
(every child expansion runs LBFGS); the obvious next-step optimisation
is a more aggressive `instantiate_iters` floor and / or pruning of
sibling children whose residual exceeds the current frontier head.

---

## PR-15 — Pauli DAG CPF v2 + retranspile + PAS + benchmark metrics (DONE)

### Staq / Python audit

See [docs/STAQ_PAULI_DAG_AUDIT.md](STAQ_PAULI_DAG_AUDIT.md) for the mapping between
staq `rotation_folding.hpp`, Python `cpf_global.py`, and qsyn types.

### New modules

| File | Role |
|--|--|
| `src/tableau/pauli_dag/pauli_dag.{hpp,cpp}` | Linear stream; global label accumulate / zero-prune |
| `src/tableau/pauli_dag/timeline.{hpp,cpp}` | Flat timeline import/export |
| `src/tableau/pauli_dag/rotation_merge.{hpp,cpp}` | staq `try_merge` + `commute_left` |
| `src/tableau/pauli_dag/staq_fold.{hpp,cpp}` | Backward `fold_forward` on timeline |
| `src/tableau/pauli_dag/pauli_dag_graph.{hpp,cpp}` | Explicit DAG (nodes, dependency/merge edges) |
| `src/tableau/pauli_dag/dag_fold.{hpp,cpp}` | Full fold: staq → graph → prune → `global_fold` |
| `tests/src/tableau/cpf_tests.cpp` | Catch2 incl. `staq_fold cx rz pair` |

### Pipeline changes

- `CpfPipelineOptions::use_dag_fold` (default **true**), `max_rounds` (default **3**).
- `run_cpf_pipeline` outer retranspile loop (Python `cpf_global_from_rz`).
- CLI: `--no-dag-fold`, `--rounds N` on `cpf-pipeline` / `qcpfq` / `cpf-optimize`.
- `U3CxCompileOptions::use_pas` + `--pas` on `to-u3cx` / pipeline.
- `scripts/cpf_bench.py`: columns `rot before→after`, `out RZ`.

### Follow-ups (still open)

- Explicit graph-structured Pauli DAG merges (Cole thesis) beyond the linear stream.
- PauliOpt integration.
- `tableau optimize cpf-dag` CLI alias (optional).
