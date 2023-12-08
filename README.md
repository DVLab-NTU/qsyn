![license](https://img.shields.io/github/license/DVLab-NTU/qsyn?style=plastic)
![stars](https://img.shields.io/github/stars/DVLab-NTU/qsyn?style=plastic)
![contributors](https://img.shields.io/github/contributors/DVLab-NTU/qsyn?style=plastic)
![release-date](https://img.shields.io/github/release-date-pre/DVLab-NTU/qsyn?style=plastic)
![pr-welcome](https://img.shields.io/badge/PRs-welcome-green?style=plastic)
![g++-11](https://img.shields.io/badge/g++-≥11-blue?style=plastic)
![clang++-16](https://img.shields.io/badge/clang++-≥16-blueviolet?style=plastic)

# Qsyn: An End-to-End Quantum Circuit Synthesis Framework

![](https://i.imgur.com/wKg5cQO.jpg)

![](https://i.imgur.com/KeliAHn.png)

<!-- ![example branch parameter](https://github.com/DVLab-NTU/qsyn/actions/workflows/build-and-test.yml/badge.svg)
 -->

## Introduction

`qsyn` is a `C++`-based growing software system for synthesizing, optimizing, and verifying quantum circuits to aid the development of quantum computing. `qsyn` implements scalable quantum circuit optimization by combining ZX-Calculus and technology mapping.

`qsyn` provides an experimental implementation of optimization algorithms and a programming environment for simulation or building similar applications. Future development will focus on enhancing the optimization and qubit mapping routines, adding support to synthesize from arbitrary unitaries, and adding verification functionalities.

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
   qsyn v0.6.2 - Copyright © 2022-2023, DVLab NTUEE.
   Licensed under Apache 2.0 License.
   qsyn>
  ```

- To see what commands are available, type `help` in the command-line interface.

  ```sh
  qsyn> help
  ```

- To see the help message of a specific command, type `<command> -h`.

  ```sh
  qsyn> qcir read -h
  ```

- You can also let `qsyn` to execute a sequence of commands by passing a DOFILE to `qsyn`:

  ```sh
  ❯ ./qsyn -f examples/synth.dof
  qsyn v0.6.2 - DVLab NTUEE.
  Licensed under Apache 2.0 License.
  qsyn> qcir read benchmark/zx/tof3.zx
  ```

  Some example DOFILEs are provided under `examples/`. You can also write your own DOFILEs to automate your workflow.

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

## License

`qsyn` is licensed under the
[Apache License 2.0](https://github.com/DVLab-NTU/qsyn/blob/main/LICENSE).

Certain functions of `qsyn` is enabled by a series of third-party libraries. For a list of these libraries, as well as their license information, please refer to [this document](/vendor/README.md).
