![license](https://img.shields.io/github/license/DVLab-NTU/qsyn?style=plastic)
![stars](https://img.shields.io/github/stars/DVLab-NTU/qsyn?style=plastic)
![contributors](https://img.shields.io/github/contributors/DVLab-NTU/qsyn?style=plastic)
![release-date](https://img.shields.io/github/release-date-pre/DVLab-NTU/qsyn?style=plastic)
![pr-welcome](https://img.shields.io/badge/PRs-welcome-green?style=plastic)
![g++-11](https://img.shields.io/badge/g++-≥11-blue?style=plastic)
![clang++-16](https://img.shields.io/badge/clang++-≥16-blueviolet?style=plastic)

# Qsyn: A Developer-Friendly Quantum Circuit Synthesis Framework for NISQ Era and Beyond

![](https://i.imgur.com/wKg5cQO.jpg)

<!-- ![example branch parameter](https://github.com/DVLab-NTU/qsyn/actions/workflows/build-and-test.yml/badge.svg) -->

## Overview

Qsyn is a developer-friendly `C++`-based Quantum Circuit Compilation framework to aid further development in this field. The main contributions of Qsyn are:

- Providing a unified and user-friendly developing environment so researchers and developers can efficiently prototype, implement, and thoroughly evaluate their QCS algorithms with standardized tools, language, and data structures.
- Assisting the developers with a robust and intuitive interface that can access low-level data directly to provide developers with unique insights into the behavior of their algorithms during runtime.
- Inforcing robust quality-assurance practices, such as regression tests, continuous integration-and-continuous delivery (CI/CD) flows, linting, etc. These methodologies ensure that we provide reliable and efficient functionalities and that new features adhere to the same quality we strive for.

![](https://i.imgur.com/DjqGlU5.png)

Some of the future work for Qsyn includes:

- Improving high-level synthesis: A well-designed high-level synthesis algorithm can yield efficient high-level circuits and adapt to the qubit resource constraint, simplifying the subsequent synthesis steps.
- More flexible gate-level synthesis: We aim to optimize multiple metrics simultaneously and characterize the trade-off between them. Device-aware synthesis strategies are also promising for further compacting the resulting circuits.
- Beyond NISQ devices: Fault-tolerant quantum architectures require decomposing the Clifford+T gate set and introducing new Clifford gate models. Distributed devices are also a common theme for realizing quantum advantage, necessitating
  algorithms targeting specifically for these devices.

## Getting Started

### System Requirements

- **CMake**: 3.29 or higher. Check with `cmake --version`; upgrade via your package manager (e.g. `brew install cmake` on macOS, or install from [cmake.org](https://cmake.org/download/)).
- **C++**: C++20. We support compilation with (1) **g++-11** or above or (2) **clang++-16** or above. We regularly perform build tests for the two compilers.
- **BLAS/LAPACK**: OpenBLAS and LAPACK are required (see platform-specific steps below).

### Installation

Clone the repository to your local machine by running

```sh
git clone https://github.com/DVLab-NTU/qsyn.git
cd qsyn
```

Then, follow the instructions below to install the dependencies and build `qsyn`.

Alternatively, you can try out `qsyn` in a containerized environment by running

```sh
docker run -it --rm dvlab/qsyn:latest
```

if you have Docker installed on your machine.

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

To build `qsyn`, follow the instructions below:

<details>
<summary>For Linux Users</summary>
    
You'll probably need to install `OpenBLAS` and `LAPACK` libraries. For Ubuntu, you can install them by running

```sh
sudo apt install libopenblas-dev
sudo apt install liblapack-dev
```

If you are tech-savvy enough to be using a different Linux distribution, we're confident that you can figure out how to install these libraries 😉

Then, run the following commands to build `qsyn`:

```sh
make -j8
```

This uses the default compiler (gcc/g++) and triggers the CMake build. To override the compiler, set `CC` and `CXX` in the environment or in a local override file (see **Local overrides** below).

</details>

<details>
<summary>For MacOS Users</summary>
    
Since Qsyn uses C++20 features not fully supported by Apple Clang, install a C++20-capable compiler. Options:

1. **LLVM (clang++)** via Homebrew:
   ```sh
   brew install llvm
   ```
   After installation, follow `brew`’s instructions to use LLVM’s `clang++` (e.g. add its `bin` to `PATH`).

   > **Note:** Use `llvm@18` on some device may work. The unversioned `llvm` formula installs the latest LLVM (currently 20+), which has a known `consteval`/`FMT_STRING` incompatibility that causes build failures with the `fmt` version used by qsyn.

2. **GCC** via Homebrew (recommended if you see fmt/consteval or similar build errors with LLVM):
   ```sh
   brew install gcc
   ```
   Then use GCC for the build by creating a `.env.local` (see **Local overrides** below) with:
   ```make
   CC := $(shell brew --prefix gcc)/bin/gcc-15
   CXX := $(shell brew --prefix gcc)/bin/g++-15
   ```
   (Replace `15` with your installed GCC version if different.)

Install **OpenBLAS**:

```sh
brew install openblas
```

Then build:

```sh
make -j8
```

</details>

<details>
<summary>Local overrides (.env.local)</summary>

To keep machine-specific settings (e.g. compiler path) out of the repo, you can use an optional **`.env.local`** file at the project root. The Makefile will include it only if the file exists; it is ignored by git (see `.gitignore`).

Use it to override `CC`, `CXX`, or other variables used by the build. Copy the example and edit as needed:

```sh
cp .env.local.example .env.local
# edit .env.local (e.g. set CC and CXX for your compiler)
```

Example **`.env.local`** (Makefile syntax; use `:=` for your paths):

```make
# Optional: override compilers (e.g. for macOS with Homebrew GCC)
CC  := /opt/homebrew/bin/gcc-15
CXX := /opt/homebrew/bin/g++-15
```

Then run `make -j8` as usual; the build will use these values.

</details>

<details>
<summary>Docker Builds</summary>

You can run `qsyn` without building (Docker image has a pre-built binary):

```sh
docker run -it --rm dvlab/qsyn:latest
```

To build `qsyn` inside a container on your machine:

```sh
make build-docker
make run-docker
```

This requires Docker and uses the image defined in `docker/dev.Dockerfile`. Note: the image must provide **CMake ≥ 3.29**; if `make build-docker` fails with a CMake version error, the base image may need to be updated.

</details>

### Run

- After successful compilation, run `./qsyn` and enter Qsyn's command-line interface:

  ```sh
   ./qsyn
   qsyn v0.8.1 - Copyright © 2022-2024, DVLab NTUEE.
   Licensed under Apache 2.0 License. Release build.
   qsyn>
  ```

- To see what commands are available, type `help` in the command-line interface.

  ```sh
  qsyn> help
  ```

- To see the help message of a specific command, type `<command> -h` or `<command> --help`.

  ```sh
  qsyn> qcir read -h
  ```

- You can also let `qsyn` to execute a sequence of commands by passing a script to `qsyn`:

  ```sh
  ./qsyn -v examples/synth.dof
  qsyn v0.8.1 - Copyright © 2022-2024, DVLab NTUEE.
  Licensed under Apache 2.0 License. Release build.
  qsyn> qcir read benchmark/SABRE/large/adr4_197.qasm
  # further executing commands...
  ```

  Some example scripts are provided under `examples/`. You can also write your own scripts to automate your workflow.

- `qsyn` also supports reading commands from scripts. For example, we also provided a ZX-calculus-based optimization flow
  by executing the following script in the project folder `examples/zxopt.qsyn`:

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
  ./qsyn examples/zxopt.qsyn benchmark/SABRE/large/adr4_197.qasm
  ```

  This runs the ZX-calculus-based synthesis on the circuit in `benchmark/SABRE/large/adr4_197.qasm`.

- If you're using `qsyn` for the first time, a config file will be created under `~/.config/qsynrc` and can be used to store your aliases, variables, etc. If you lost this file, you can use the command `create-qsynrc` to recreate it.

- For more options, please refer to the help message of `qsyn` by running

  ```sh
  ./qsyn -h
  ```

### Testing

We have provided some test scripts as integrated tests as well as demonstration of use. These scripts are Located under `tests/<section>/<subsection>/dof/`.

- To run a test script and compare the result to the reference, type

  ```sh
  ./scripts/RUN_TESTS <path/to/test> -d
  ```

- To update the reference to a test script, type

  ```sh
  ./scripts/RUN_TESTS <path/to/test> -u
  ```

- You may also run all test scripts by running

  ```sh
  make test
  ```

- To run test in a containerized environment, run

  ```sh
  make test-docker
  ```

  Notice that if you use a different BLAS or LAPACK implementation to build `qsyn`, be expect that some of the test scripts may produce different results.

## Functionalities

### Software architecture

The core interaction with `qsyn` is facilitated through its command-line interface (CLI), which processes user input, handles command execution, and manages error reporting. Developers can add additional commands and integrate into the CLI. New strategy can be added without compromising to existing data structure, as all modifications are funneled through well-defined public interfaces.

`qsyn` supports real time storage of multiple quantum circuits and intermediate representations by the `<dt>` checkout command. Users can take snapshots at any time in the synthesis process and switch to an arbitrary version of stored data.

This architecture is central to qsyn’s flexibility and extensibility. It segregates responsibilities and simplifies command implementations while enhancing its capability as a research tool in quantum circuit synthesis.

### High-level Synthesis

`qsyn` can process various specifications for quantum circuits by supporting syntheses from Boolean oracles and unitary matrices.

| Command       | Description                        |
| :------------ | :--------------------------------- |
| qcir oracle   | ROS Boolean oracle synthesis flow  |
| convert ts qc | Gray-code unitary matrix synthesis |

### Gate-level Synthesis

`qsyn` also adapt to different optimization targets by providing different routes to synthesize low-level quantum circuits.

#### Via ZX-calculus

| Command       | Description                           |
| :------------ | :------------------------------------ |
| qzq           | ZX-calculus-based synth routine       |
| convert qc zx | convert quantum circuit to ZX-diagram |
| zx opt        | fully reduce ZX diagram               |
| convert zx qc | convert ZX-diagram to quantum circuit |
| qc opt        | basic optimization passes             |

#### Via Tableau

| Command          | Description                           |
| :--------------- | :------------------------------------ |
| qtablq           | tableau-based synth routine           |
| convert qc tabl  | convert quantum circuit to tableau    |
| tabl opt full    | iteratively apply the following three |
| tabl opt tmerge  | phase-merging optimization            |
| tabl opt hopt    | internal H-gate optimization          |
| tabl opt ph todd | TODD optimization                     |
| convert tabl qc  | convert tableau to quantum circuit    |

### Device Mapping

`qsyn` can target a wide variety of quantum devices by addressing their available gate sets and topological constraints.
| Command | Description |
| :----- | :---- |
| device read | read info about a quantum device |
| qc translate | library-based technology mapping |
| qc opt -t | technology-aware optimization passes |
| duostra | Duostra qubit mapping for depth or #SWAPs |

### Data Access and Utilities

`qsyn` provides various data representations for quantum logic. `<dt>` stands for any data representation type, including quantum circuits, ZX diagrams, Tableau, etc.
| Command | Description |
| :----- | :---- |
| `<dt>` list | list all `<dt>`s |
| `<dt>` checkout | switch focus between `<dt>`s |  
 | `<dt>` print | print `<dt>` information |
| `<dt>` <new \| delete> | add a new/delete a `<dt>` |
| `<dt>` <read \| write> | read and write `<dt>` |
| `<dt>` equiv | verify equivalence of two `<dt>`s |
| `<dt>` draw | render visualization of `<dt>` |
| convert `<dt1>` `<dt2>` | convert from `<dt1>` to `<dt2>` |

There are also some extra utilities:
| Command | Description |
| :----- | :---- |
| alias | set or unset aliases |
| help | display helps to commands |
| history | show or export command history |
| logger | control log levels |
| set | set or unset variables |
| usage | show time/memory usage |

## License

`qsyn` is licensed under the
[Apache License 2.0](https://github.com/DVLab-NTU/qsyn/blob/main/LICENSE).

Certain functions of `qsyn` is enabled by a series of third-party libraries. For a list of these libraries, as well as their license information, please refer to [this document](/vendor/README.md).

## Resources

* Quantum circuit benchmark: [qsyn-benchmark](https://github.com/DVLab-NTU/qsyn-benchmark)
 
## Reference
If you are using Qsyn for your research, it will be greatly appreciated if you cite this publication:

```
@misc{Lau_Qsyn_2024,
Author = {Mu-Te Lau and Chin-Yi Cheng and Cheng-Hua Lu and Chia-Hsu Chuang and Yi-Hsiang Kuo and Hsiang-Chun Yang and Chien-Tung Kuo and Hsin-Yu Chen and Chen-Ying Tung and Cheng-En Tsai and Guan-Hao Chen and Leng-Kai Lin and Ching-Huan Wang and Tzu-Hsu Wang and Chung-Yang Ric Huang},
Title = {Qsyn: A Developer-Friendly Quantum Circuit Synthesis Framework for NISQ Era and Beyond},
Year = {2024},
Eprint = {arXiv:2405.07197},
}
```
