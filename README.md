![license](https://img.shields.io/github/license/DVLab-NTU/qsyn?style=plastic)
![stars](https://img.shields.io/github/stars/DVLab-NTU/qsyn?style=plastic)
![contributors](https://img.shields.io/github/contributors/DVLab-NTU/qsyn?style=plastic)
![release-date](https://img.shields.io/github/release-date-pre/DVLab-NTU/qsyn?style=plastic)
![pr-welcome](https://img.shields.io/badge/PRs-welcome-green?style=plastic)
![g++-11](https://img.shields.io/badge/g++-≥11-blue?style=plastic)
![clang++-16](https://img.shields.io/badge/clang++-≥16-blueviolet?style=plastic)

# Qsyn: A Developer-Friendly Quantum Circuit Synthesis Framework for NISQ Era and Beyond

![](https://i.imgur.com/wKg5cQO.jpg)

<!-- ![example branch parameter](https://github.com/DVLab-NTU/qsyn/actions/workflows/build-and-test.yml/badge.svg)
 -->

## Overview

Qsyn is a developer-friendly `C++`-based Quantum Circuit Compilation framework to aid further development in this field. The main contributions of Qsyn are:

* Providing a unified and user-friendly developing environment so researchers and developers can efficiently prototype, implement, and thoroughly evaluate their QCS algorithms with standardized tools, language, and data structures.
* Assisting the developers with a robust and intuitive interface that can access low-level data directly to provide developers with unique insights into the behavior of their algorithms during runtime.
* Inforcing robust quality-assurance practices, such as regression tests, continuous integration-and-continuous delivery (CI/CD) flows, linting, etc. These methodologies ensure that we provide reliable and efficient functionalities and that new features adhere to the same quality we strive for. 

![](https://i.imgur.com/DjqGlU5.png)

Some of the future work for Qsyn includes:
* Improving high-level synthesis: A well-designed high-level synthesis algorithm can yield efficient high-level circuits and adapt to the qubit resource constraint, simplifying the subsequent synthesis steps.
* More flexible gate-level synthesis: We aim to optimize multiple metrics simultaneously and characterize the trade-off between them. Device-aware synthesis strategies are also promising for further compacting the resulting circuits.
* Beyond NISQ devices: Fault-tolerant quantum architectures require decomposing the Clifford+T gate set and introducing new Clifford gate models. Distributed devices are also a common theme for realizing quantum advantage, necessitating
algorithms targeting specifically for these devices.

## Getting Started

### System Requirements

`qsyn` requires `c++-20` to build. We support compilation with (1) `g++-11` or above or (2) `clang++-16` or above. We regularly perform build tests for the two compilers.

### Installation

Clone the repository to your local machine by running

```shell!
git clone https://github.com/DVLab-NTU/qsyn.git
cd qsyn
```

Then, follow the instructions below to install the dependencies and build `qsyn`.

Or you can try out `qsyn` in a containerized environment by running

```shell!
docker run -it --rm dvlab/qsyn:latest
```

### Optional Dependencies for Visualization

Visualization functionalities of `qsyn` depend at runtime on the following dependencies. Please refer to the linked pages for installation instructions of these dependencies:

- `qiskit`, `qiskit[visualization]` for drawing quantum circuits
  - Please refer to [this page](https://qiskit.org/documentation/getting_started.html)
- `texlive` for drawing ZX-diagrams.
  - For Ubuntu:
    ```sh
    sudo apt-get install texlive-latex-base texlive-latex-extra
    ```
  - Other Platforms: please refer to [this page](https://tug.org/texlive/quickinstall.html)

### Compilation

`qsyn` uses CMake to manage the build process. To build `qsyn`, follow the instructions below:

1. Run `cmake` to build dependencies and generate Makefiles, if this step fails, you might have to install external dependencies `blas`, `lapack` or `xtensor` yourself.

   ```sh
   cmake -B build -S .
   ```

   **Note for Mac Users:** Since we use some C++20 features that are not yet supported by Apple Clang, you'll need to install another compiler yourself. We recommend installing the `llvm` toolchain with `clang++` by running

   ```sh
   brew install llvm
   ```

   Then, run the following command to force `cmake` to use the new `clang++` you installed.

   ```sh
   cmake -DCMAKE_CXX_COMPILER=$(which clang++) -B build -S .
   ```

2. Build the executable. You would want to crank up the number of threads to speed up the compilation process:

   ```sh
   cmake --build build -j 8
   ```

   You can now execute `qsyn` by running

   ```sh
    ./qsyn
   ```

   You can also build `qsyn` in a containerized environment by running

   ```sh
   make build-docker
   ```

   And run that executable by running

   ```sh
   make run-docker
   ```

### Run

- After successful compilation, open the command-line interface of `qsyn` by running

  ```sh
   ❯ ./qsyn
   qsyn v0.7.0 - Copyright © 2022-2024, DVLab NTUEE.
   Licensed under Apache 2.0 License.
   qsyn>
  ```

- To see what commands are available, type `help` in the command-line interface.

  ```sh
  qsyn> help
  ```

- To see the help message of a specific command, type `<command> -h` or  `<command> --help`.

  ```sh
  qsyn> qcir read -h
  ```

- You can also let `qsyn` to execute a sequence of commands by passing a DOFILE to `qsyn`:

  ```sh
  ❯ ./qsyn -v examples/synth.dof
  qsyn v0.7.0 - DVLab NTUEE.
  Licensed under Apache 2.0 License.
  qsyn> qcir read benchmark/zx/tof3.zx
  ```

  Some example DOFILEs are provided under `examples/`. You can also write your own DOFILEs to automate your workflow.

- `qsyn` also supports reading commands from scripts. For example, we also provided a ZX-calculus-based optimization flow
  by executing the following script in the project folder examples/zxopt.qsyn .

  ```sh
  //!ARGS INPUT
  qcir read ${INPUT}
  echo "--- pre-optimization ---"
  qcir print --stat
  // to zx -> full reduction -> extract qcir
  qc2zx; zx optimize --full; zx2qc
  // post-resyn optimization
  qcir optimize
  echo "--- post-optimization ---"
  qcir print --stat
  ```

  To run the commands in the above file, supply the script’s file path and arguments after ./qsyn :
  
  ```sh
   ❯ ./qsyn examples/zxopt.qsyn \ benchmark/SABRE/large/adr4_197.qasm
  ```
  This runs the ZX-calculus-based synthesis on the circuit in benchmark/SABRE/large/adr4_197.qasm. 

- If you're new to `qsyn`, you will be prompted to run the command `create-qsynrc` to create a configuration file for `qsyn`. This file will be stored under `~/.config/qsynrc` and can be used to store your aliases, variables, etc.

- For more options, please refer to the help message of `qsyn` by running

  ```sh
  ./qsyn -h
  ```

### Testing

We have provided some DOFILEs, i.e., a sequence of commands, to serve as functionality checks as well as demonstration of use. DOFILEs are Located under `tests/<section>/<subsection>/dof/`.

- To run a DOFILE and compare the result to the reference, type

  ```sh
  ./scripts/RUN_TESTS <path/to/test> -d
  ```

- To update the reference to a dofile, type

  ```sh
  ./scripts/RUN_TESTS <path/to/test> -u
  ```

- You may also run all DOFILEs by running

  ```sh
  make test
  ```

- To run test in a containerized environment, run

  ```sh
  make test-docker
  ```

  Notice that if you use a different BLAS or LAPACK implementation to build `qsyn`, some of the DOFILEs may produce different results, which is expected.

## Functionalities

### Software architecture

  The core interaction with `qsyn` is facilitated through its command-line interface (CLI), which processes user input, handles command execution, and manages error reporting. Developers can add additional commands and integrate into the CLI. New strategy can be added without compromising to existing data structure, as all modifications are funneled through well-defined public interfaces.

  `qsyn` supports real time storage of multiple quantum circuits and intermediate representations by the `<dt>` checkout command. Users can take snapshots at any time in the synthesis process and switch to an arbitrary version of stored data. 

  This architecture is central to qsyn’s flexibility and extensibility. It segregates responsibilities and simplifies command implementations while enhancing its capability as a research tool in quantum circuit synthesis.

### High-level Synthesis
  
  `qsyn` can process various specifications for quantum circuits by supporting syntheses from Boolean oracles and unitary matrices.

  | Command | Description |
  | :-----  | :----       |
  | qcir oracle | ROS Boolean oracle synthesis flow      |
  | convert ts qc       | Gray-code unitary matrix synthesis     |

### Gate-level Synthesis
 `qsyn` also adapt to different optimization targets by providing  different routes to synthesize low-level quantum circuits.

#### Via ZX-calculus
  | Command  | Description |
  | :-----   | :----       |
  | qzq      | ZX-calculus-based synth routine            |
  | convert qc zx    | convert quantum circuit to ZX-diagram           |
  | zx opt   | fully reduce ZX diagram                    |
  | convert zx qc    | convert ZX-diagram to quantum circuit           |
  | qc opt   | basic optimization passes                  |

#### Via Tableau
  | Command  | Description |
  | :-----   | :----       |
  | qtablq   | tableau-based synth routine                |
  | convert qc tabl  | convert quantum circuit to tableau         |
  | tabl opt full | iteratively apply the following three |
  | tabl opt tmerge | phase-merging optimization          |
  | tabl opt hopt | internal H-gate optimization          |
  | tabl opt ph todd | TODD optimization                  |
  | convert tabl qc  | convert tableau to quantum circuit         |

### Device Mapping
  `qsyn` can target a wide variety of quantum devices by addressing their available gate sets and topological constraints.
  | Command      | Description |
  | :-----       | :----       |
  | device read  | read info about a quantum device       |
  | qc translate | library-based technology mapping       |
  | qc opt       |-t technology-aware optimization passes |
  | duostra  | Duostra qubit mapping for depth or #SWAPs  |


### Data Access and Utilities
  `qsyn` provides various data representations for quantum logic. `<dt>` stands for any data representation type, including quantum circuits, ZX diagrams, Tableau, etc.
  | Command             | Description |
  | :-----              | :----       |
  | `<dt>` list           | list all `<dt>`s                  |
  | `<dt>` checkout       | switch focus between `<dt>`s      |  
  | `<dt>` print          | print `<dt>` information          |
  | `<dt>` new/delete     | add a new/delete a `<dt>`         |
  | `<dt>` read/write     | read and write `<dt>`             |
  | `<dt>` equiv          | verify equivalence of two `<dt>`s |
  | `<dt>` draw           | render visualization of `<dt>`    |
  | convert `<dt1>` `<dt2>` | convert from `<dt1>` to `<dt2>`     |

  There are also some extra utilities: 
  | Command   | Description |
  | :-----    | :----       |
  | alias     | set or unset aliases            |
  | help      | display helps to commands       |
  | history   | show or export command history  |
  | logger    | control log levels              |
  | set       | set or unset variables          |
  | usage     | show time/memory usage          |




## License

`qsyn` is licensed under the
[Apache License 2.0](https://github.com/DVLab-NTU/qsyn/blob/main/LICENSE).

Certain functions of `qsyn` is enabled by a series of third-party libraries. For a list of these libraries, as well as their license information, please refer to [this document](/vendor/README.md).
 
