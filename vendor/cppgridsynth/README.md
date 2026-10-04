# cppgridsynth

`cppgridsynth` is a C++20 port of the
[`pygridsynth`](https://github.com/quantum-programming/pygridsynth) library.
It computes Clifford+T approximations of single-qubit Z-rotations
$R_z(\theta)$ to arbitrary precision $\varepsilon$ via the
Selinger-Ross gridsynth algorithm.

## Highlights

- **Arbitrary-precision arithmetic.** All algebraic-number coefficients
  are tracked exactly using the [GMP](https://gmplib.org/) library
  (`mpz_class`).  All real / floating-point computations use
  [MPFR](https://www.mpfr.org/) through a thin
  `cppgridsynth::MPFloat` wrapper that preserves the `mpmath`-style
  semantics relied upon by the original Python implementation.
- **Algebraic ring data structures** matching the pygridsynth design:
  `ZRootTwo` ($\mathbb Z[\sqrt 2]$), `DRootTwo` ($\mathbb D[\sqrt 2]$),
  `ZOmega` ($\mathbb Z[\omega]$, $\omega = e^{i\pi/4}$), and
  `DOmega` ($\mathbb D[\omega]$).
- **qsyn-style polymorphic gate hierarchy.** Each gate is an
  `Operation` (the qsyn base class equivalent) carrying `name`,
  `qubits`, and a `matrix()` materialiser.  Concrete gates – `HGate`,
  `TGate`, `SGate`, `SXGate`, `WGate`, `RxGate`, `RzGate`, `CxGate` –
  refine the base type and are stored polymorphically through
  `std::shared_ptr<Operation>` in a `QuantumCircuit`.
- **Faithful algorithm port.** Includes the upright-form reduction,
  ODGP / TDGP solvers, Diophantine equation solver (Pollard-Rho
  factoring, Cipolla-style square roots over $\mathbb F_{p^2}$), and
  the Matsumoto-Amano normal form for the final gate-list compaction.
- **CLI** mirroring the pygridsynth tool.

## Building

### Requirements

- A C++20 compiler (Clang ≥ 16 or GCC ≥ 11)
- GNU Make
- [GMP](https://gmplib.org/) ≥ 6.2 (with C++ bindings, i.e. `gmpxx`)
- [MPFR](https://www.mpfr.org/) ≥ 4.0

#### macOS (Homebrew)

```bash
brew install gmp mpfr
```

#### Debian / Ubuntu

```bash
sudo apt install libgmp-dev libmpfr-dev build-essential pkg-config
```

### Compile

```bash
cd cppgridsynth
make -j
```

Other useful targets:

```bash
make test          # run test_ring and test_gridsynth
make clean         # remove build/
make debug         # build with -O0 -g
make install       # install CLI to $(PREFIX)/bin (default PREFIX=/usr/local)
```

The CLI executable lands in `build/cppgridsynth`, the static library in
`build/libcppgridsynth.a`.

If GMP/MPFR are in a non-standard location, override discovery:

```bash
make GMP_DIR=/path/to/gmp MPFR_DIR=/path/to/mpfr
```

On macOS, if `c++` is Homebrew `llvm@N`, the Makefile strips LLVM include/lib
paths and passes `-Wl,-w` when linking to silence harmless `libunwind`
reexport warnings. To use Xcode's toolchain for both compile and link:

```bash
make CXX="$(xcrun -find clang++)"
```

## Command-line usage

```bash
./build/cppgridsynth <theta> <epsilon> [options]
```

Example: synthesize $R_z(0.5)$ within $10^{-10}$:

```bash
./build/cppgridsynth 0.5 1e-10 --time
```

Run `./build/cppgridsynth --help` for the full option list.

## Library usage

```cpp
#include <cppgridsynth/gridsynth.hpp>

int main() {
    cppgridsynth::GridsynthConfig cfg;
    cfg.dps = 60;
    cfg.seed = 0;

    std::string gates = cppgridsynth::gridsynth_gates("0.5", "1e-10", cfg);
    std::cout << gates << "\n";
}
```

The `gridsynth_circuit(...)` overload returns a `QuantumCircuit` of
qsyn-style `OperationPtr`s for further programmatic processing.

## Project layout

```
cppgridsynth/
├── Makefile
├── README.md
├── app/               # CLI executable
├── include/cppgridsynth/
│   ├── mpfloat.hpp        # MPFR wrapper (working-precision aware)
│   ├── mymath.hpp         # math helpers (floorlog, solve_quadratic, ...)
│   ├── ring.hpp           # ZRootTwo / DRootTwo / ZOmega / DOmega
│   ├── quantum_gate.hpp   # qsyn-style gate hierarchy
│   ├── quantum_circuit.hpp
│   ├── grid_op.hpp
│   ├── region.hpp         # Interval, Rectangle, Ellipse, ConvexSet
│   ├── odgp.hpp / tdgp.hpp / to_upright.hpp
│   ├── diophantine.hpp    # GMP-backed factoring + square roots mod p
│   ├── domega_unitary.hpp # symbolic 2x2 unitary
│   ├── normal_form.hpp    # Matsumoto-Amano normal form
│   ├── synthesis_of_cliffordT.hpp
│   ├── gridsynth.hpp      # top-level pipeline
│   ├── loop_controller.hpp
├── src/                # implementations of the headers above
└── tests/              # smoke tests
```

## Mapping from `pygridsynth`

| pygridsynth module                | cppgridsynth header / source                |
| --------------------------------- | ------------------------------------------- |
| `mymath.py`                       | `mpfloat.hpp`, `mymath.hpp`                 |
| `ring.py`                         | `ring.hpp`                                  |
| `quantum_gate.py`                 | `quantum_gate.hpp`                          |
| `quantum_circuit.py`              | `quantum_circuit.hpp`                       |
| `region.py`                       | `region.hpp`                                |
| `grid_op.py`                      | `grid_op.hpp`                               |
| `odgp.py`                         | `odgp.hpp`                                  |
| `tdgp.py`                         | `tdgp.hpp`                                  |
| `to_upright.py`                   | `to_upright.hpp`                            |
| `diophantine.py`                  | `diophantine.hpp`                           |
| `domega_unitary.py`               | `domega_unitary.hpp`                        |
| `normal_form.py`                  | `normal_form.hpp`                           |
| `synthesis_of_cliffordT.py`       | `synthesis_of_cliffordT.hpp`                |
| `gridsynth.py`                    | `gridsynth.hpp`                             |
| `loop_controller.py`              | `loop_controller.hpp`                       |
| `config.py`                       | `gridsynth.hpp::GridsynthConfig`            |
| `cli.py`                          | `app/main.cpp`                              |

## License

MIT — same as the upstream `pygridsynth`.
