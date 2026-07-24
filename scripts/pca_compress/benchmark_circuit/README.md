# NCF-paper benchmark circuits

This folder collects the 13 Hamiltonian-simulation benchmarks used by

> Yingheng Li, Xulong Tang, Paul Hovland, Ji Liu,  
> **"Non-Clifford Fusion: T-Gate Optimization for Quantum Simulation"**,  
> arXiv:[2510.13573](https://arxiv.org/abs/2510.13573) (2025), Table IV.

The NCF authors do **not** publish a stand-alone benchmark dataset. The
circuits/Hamiltonians in this folder are sourced from the closest publicly
available open-source artifacts, principally the [Phoenix
repo](https://github.com/iqubit-org/phoenix), which integrates the
benchmarks of [Paulihedral (ASPLOS'22)](https://arxiv.org/abs/2109.03371).

## Layout

```
benchmark_circuit/
├── gen_benchmark_qasm.py       # one-stop generator (lattice + molecule)
├── lattice/                    # generated Ising / Heisenberg QASMs
├── molecule/                   # generated LiH / H2O / N2 / H2S / CO2 QASMs
├── chemistry/                  # raw chemistry circuits from Phoenix
│   ├── hamlib/                 #   Hamlib QASM + JSON (single Trotter step)
│   └── uccsd/                  #   UCCSD ansatz QASM + JSON
└── source/                     # third-party source files
    ├── heisenberg.py, ising.py, molecule.py, mypauli.py, ...
    ├── benchmark/              #   shim package needed to unpickle
    └── paulihedral_data/       #   PySCF-generated Hamiltonians (.pickle)
```

## NCF Table IV ↔ files in this folder

| NCF benchmark        | qubits | #Pauli (NCF) | #Pauli (here) | source |
|----------------------|:------:|:------------:|:-------------:|--------|
| LiH                  | 12     | 630          | 630           | `molecule/LiH.qasm` *(PySCF via `chemistry/hamlib/HF-BK12.json`)* |
| H2O                  | 14     | 1085         | 1086          | `molecule/H2O.qasm` *(`source/paulihedral_data/H2O.pickle`)* |
| N2                   | 20     | 2950         | 2951          | `molecule/N2.qasm` *(`source/paulihedral_data/N2.pickle`)* |
| H2S                  | 22     | 6245         | 4582          | `molecule/H2S.qasm` *(`source/paulihedral_data/H2S.pickle`, **different active space**)* |
| CO2                  | 30     | 16121        | 16154         | `molecule/CO2.qasm` *(`source/paulihedral_data/CO2.pickle`)* |
| Ising-2D-30          | 30     | 79           | 79            | `lattice/Ising-2D-30.qasm` |
| Ising-2D-60          | 60     | 164          | 164           | `lattice/Ising-2D-60.qasm` |
| Ising-3D-30          | 30     | 89           | 89            | `lattice/Ising-3D-30.qasm` |
| Ising-3D-60          | 60     | 193          | 193           | `lattice/Ising-3D-60.qasm` |
| Heisenberg-2D-30     | 30     | 147          | 147           | `lattice/Heisenberg-2D-30.qasm` |
| Heisenberg-2D-60     | 60     | 312          | 312           | `lattice/Heisenberg-2D-60.qasm` |
| Heisenberg-3D-30     | 30     | 177          | 177           | `lattice/Heisenberg-3D-30.qasm` |
| Heisenberg-3D-60     | 60     | 399          | 399           | `lattice/Heisenberg-3D-60.qasm` |

Note: NCF "LiH" (12 qubits, 630 Pauli strings) does not match the active
space of `paulihedral_data/LiH_UCCSD.pickle` (8 qubits, 144 strings).
We instead use the Phoenix Hamlib `HF-BK12` Hamiltonian, which has
exactly 12 qubits and 630 Pauli strings -- the same shape as NCF's LiH.
The same is true for any other entry where the publicly reachable
artifacts disagree with NCF in active-space size; H2S is the only case
where the discrepancy is large and we keep it as a documented mismatch.

## Lattice geometry

The lattice configurations were chosen so that the resulting
Pauli-string count matches NCF Table IV exactly (verified by
`gen_benchmark_qasm.py --verify`):

| name | model | grid `(w,h[,l])` | qubits | edges | total Pauli strings |
|------|:------|:-----------------|:------:|:-----:|:-------------------:|
| Ising-2D-30      | TFIM ZZ + X      | (4, 5)         | 30 |  49 |  79 = 49 +    30 |
| Ising-2D-60      | TFIM ZZ + X      | (5, 9)         | 60 | 104 | 164 = 104 +   60 |
| Ising-3D-30      | TFIM ZZ + X      | (1, 4, 2)      | 30 |  59 |  89 = 59 +    30 |
| Ising-3D-60      | TFIM ZZ + X      | (2, 3, 4)      | 60 | 133 | 193 = 133 +   60 |
| Heisenberg-2D-30 | XX + YY + ZZ     | (4, 5)         | 30 |  49 | 147 = 3·49        |
| Heisenberg-2D-60 | XX + YY + ZZ     | (5, 9)         | 60 | 104 | 312 = 3·104       |
| Heisenberg-3D-30 | XX + YY + ZZ     | (1, 4, 2)      | 30 |  59 | 177 = 3·59        |
| Heisenberg-3D-60 | XX + YY + ZZ     | (2, 3, 4)      | 60 | 133 | 399 = 3·133       |

Hamiltonians used:

- **TFIM (Ising):**  `H = -sum_<ij> J_ij Z_i Z_j  -  sum_i h_i X_i`
  with smooth spatial variation in `J_ij`, `h_i` (default scales
  `J0=0.52`, `h0=1.15`, modulation amplitudes ~20–28%).
- **Heisenberg (XXZ):**  `H = sum_<ij> (J_xy (X_i X_j + Y_i Y_j) + J_z Z_i Z_j)`
  with per-edge anisotropy between XX/YY and ZZ.

The QASM circuits encode a single Trotter step.  Default **`dt = 0.05`**
(continuous non-uniform `rz` angles via `rz(2 * w_j * dt)`).  Use
`--uniform-lattice` to recover the legacy toy model (all coeffs = 1,
`dt = 1`, every rotation `rz(2.0)`).

## Reproducing / regenerating

```bash
# verify only:
python gen_benchmark_qasm.py --verify

# regenerate everything:
python gen_benchmark_qasm.py --all

# only Heisenberg-3D / specific molecules:
python gen_benchmark_qasm.py --lattice
python gen_benchmark_qasm.py --molecule H2O N2

# different time-step:
python gen_benchmark_qasm.py --all --dt 0.1

# legacy uniform lattice (old rz(2.0) everywhere):
python gen_benchmark_qasm.py --lattice --uniform-lattice
```

**Molecule angles:** all five molecules are re-emitted from PySCF
Hamiltonian coefficients (LiH from `HF-BK12.json`, others from
`paulihedral_data/*.pickle`), so `rz` angles are non-uniform like the
upstream Phoenix / Paulihedral benchmarks.

The script has **no external dependencies** beyond the Python standard
library + `numpy` (used transitively by `mypauli.py`); in particular it
does **not** require Qiskit or PySCF for the lattice generators.
Loading the molecule pickles only requires `numpy`.

## Sources

- **Lattice:** generated locally by `gen_benchmark_qasm.py` to match
  NCF Table IV exactly.  The geometric formulae and Pauli ordering
  follow the conventions of Paulihedral's `benchmark/heisenberg.py`
  and `benchmark/ising.py`.
- **Molecule pickles** in `source/paulihedral_data/`: copied verbatim
  from the [Phoenix repo](https://github.com/iqubit-org/phoenix) at
  `paulihedral/benchmark/data/`.  Each pickle is a `list[list[pauliString]]`
  produced by Paulihedral via PySCF + Qiskit Bravyi–Kitaev mapping.
- **Hamlib chemistry QASM/JSON** in `chemistry/hamlib/`: copied from
  the Phoenix repo at `benchmarks/hamlib_qasm/chemistry/` and
  `benchmarks/hamlib_json/chemistry/`.  These are pre-compiled single
  Trotter steps for the Hamlib library Hamiltonians.
- **UCCSD QASM/JSON** in `chemistry/uccsd/`: copied from
  `benchmarks/uccsd_qasm/` and `benchmarks/uccsd_json/`.  These are
  UCCSD ansatz circuits (LiH/CH2/NH × {complete, frozen-core} ×
  {BK, JW, parity}).

## Using these with the PCA-compress experiment

Each QASM file in `lattice/` and `molecule/` is a flat list of Pauli
rotations (single Trotter step).  Feed them to the existing CPF
pipeline used by `pca_compress.py`, e.g.:

```text
qcir read scripts/pca_compress/benchmark_circuit/lattice/Heisenberg-3D-30.qasm
qcir to-zyz -r
qcir to-tableau --fold
tableau print
qcir from-tableau -r
qcir write scripts/pca_compress/benchmark_circuit/dump/Heisenberg-3D-30_collapsed.qasm
```

Then run `pca_compress.py` against the collapsed QASM the same way the
`qaoa5` experiment was driven.

## License / attribution

The third-party files in `source/` and `chemistry/` are reproduced from
the [Phoenix](https://github.com/iqubit-org/phoenix) repository, which
itself integrates code from
[Paulihedral](https://arxiv.org/abs/2109.03371). Please cite both
papers (and the NCF paper) if you use these benchmarks in published
work.
