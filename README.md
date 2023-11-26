![license](https://img.shields.io/github/license/DVLab-NTU/qsyn?style=plastic)
![stars](https://img.shields.io/github/stars/DVLab-NTU/qsyn?style=plastic)
![contributors](https://img.shields.io/github/contributors/DVLab-NTU/qsyn?style=plastic)
![pr-welcome](https://img.shields.io/badge/PRs-welcome-green?style=plastic)
![g++-10](https://img.shields.io/badge/g++-≥10-blue?style=plastic)
![clang++-16](https://img.shields.io/badge/clang++-≥16-blueviolet?style=plastic)

# Qsyn: An End-to-End Quantum Circuit Synthesis Framework

![](https://i.imgur.com/wKg5cQO.jpg)

![](https://i.imgur.com/KeliAHn.png)

<!-- ![example branch parameter](https://github.com/DVLab-NTU/qsyn/actions/workflows/build-and-test.yml/badge.svg)
 -->

## Introduction

`qsyn` is a `C++`-based growing software system for synthesis, optimization and verification of quantum circuits for to aid the development of quantum computing. `qsyn` implements scalable quantum circuits optimization by combining ZX-Calculus and technology mapping.

`qsyn` provides an experimental implementation of optimization algorithms and a programming environment for simulation or building similar applications. Future development will focus on enhancing the optimization and qubit mapping routines, adding support to synthesize from arbitrary unitaries, as well as adding in verification functionalities.

## Getting Started

### System Requirements

`qsyn` requires `c++-20` to build. We support compilation with (1) `g++-10` or above, or (2) `clang++-16` or above. We regularly perform build tests for the two compilers.

### Installation

```shell!
git clone https://github.com/DVLab-NTU/qsyn.git
cd qsyn
```

Then, you can follow the instructions below to install the dependencies and build `qsyn`.

Or you can try out `qsyn` in a containerized environment by running

```sh
docker run -it --rm dvlab/qsyn
```

### Optional Dependencies for Visualization

Visualization functionalities of `qsyn` depends at runtime on the following dependencies. Please refer to the linked pages for installation instructions:

- `qiskit`, `qiskit[visualization]` for drawing quantum circuits
  - Please refer to [this page](https://qiskit.org/documentation/getting_started.html)
- `texlive` for drawing ZX-diagrams.
  - For Ubuntu:
    ```sh
    sudo apt-get install texlive-latex-base texlive-latex-extra
    ```
  - Other Platforms: please refer to [this page](https://tug.org/texlive/quickinstall.html)

### Compilation

`qsyn` uses CMake to manage dependencies:

1. create a `build` directory to store CMake artifacts

```sh
mkdir build
cd build
```

2. run CMake to generate Makefiles, if this step fails, you might have to install `blas` and `lapack` libraries.

```sh
cmake ..
```

**Note for Mac Users:** Since we use some C++20 features that are not yet supported by Apple Clang, you'll need to install another compiler yourself. We recommand installing the `llvm` toolchain with `clang++` by running

```sh
brew install llvm
```

Then, run the following command to force `cmake` to use the new `clang++` you installed.

```sh
cmake .. -DCMAKE_CXX_COMPILER=$(which clang++)
```

3. run `make` to build up the executable, you would want to crank up the number of threads to speed up the compilation process

```sh
cmake --build . -j16
# or
make -j16
```

You can also build `qsyn` in a containerized environment by running

```sh
docker run -it --rm -v $(pwd):/qsyn dvlab/qsyn-env
cd /qsyn
```

Then, you can follow the instructions above to build `qsyn` in the container.

### Run

- After successful compilation, you can call the command-line interface of `Qsyn` where you can execute commands implemented into `Qsyn`.

  ```sh
   ❯ ./qsyn
   DV Lab, NTUEE, Qsyn 0.5.1
   qsyn>
  ```

- To run the demo program, you can provide a file containing commands. For example:

  ```sh
  ❯ ./qsyn -f tests/demo/demo/dof/tof_3.dof
  DV Lab, NTUEE, Qsyn 0.5.1
  qsyn> verb 0
  Note: verbose level is set to 0

  qsyn> zxgread benchmark/zx/tof3.zx

  qsyn> zxgs -freduce

  qsyn> zxgp
  Graph 0( 3 inputs, 3 outputs, 17 vertices, 19 edges )

  qsyn> qq -f
  ```

- The same result can be produced by running in the command-line mode:

  ```sh
  ❯ ./qsyn
  DV Lab, NTUEE, Qsyn 0.4.0
  qsyn> dofile tests/demo/demo/dof/tof_3.dof

  qsyn> verb 0
  Note: verbose level is set to 0

  qsyn> zxgread benchmark/zx/tof3.zx

  qsyn> zxgs -freduce

  qsyn> zxgp
  Graph 0( 3 inputs, 3 outputs, 17 vertices, 19 edges )

  qsyn> qq -f
  ```

### Testing

We have provided some DOFILEs, i.e., a sequence of commands, to serve as functionality checks as well as demonstration of use. DOFILEs are Located under `tests/<section>/<subsection>/dof/`.

- To run a DOFILE and compare the result to the reference, type

  ```sh
  ./RUN_TESTS <path/to/test> -d
  ```

- To update the reference to a dofile, type

  ```sh
  ./RUN_TESTS <path/to/test> -u
  ```

- You may also run all DOFILEs by running

  ```sh
  ./RUN_TESTS
  ```

- To run test in a containerized environment, run

  ```sh
  ./RUN_TESTS_DOCKER
  ```

  All arguments are the same as `RUN_TESTS`.

  Notice that if you use a different BLAS or LAPACK implementation to build `Qsyn`, some of the DOFILEs may produce different results, which is to be expected.

## License

`qsyn` is licensed under the
[Apache License 2.0](https://github.com/DVLab-NTU/qsyn/blob/main/LICENSE).

Certain functions of `qsyn` is enabled by a series of third-party libraries. For a list of these libraries, as well as their license information, please refer to [this document](/vendor/List-of-Used-Libraries.md).
