
![license](https://img.shields.io/github/license/ric2k1/qsyn?style=plastic)
![stars](https://img.shields.io/github/stars/ric2k1/qsyn?style=plastic)
![contributors](https://img.shields.io/github/contributors/ric2k1/qsyn?style=plastic)
![pr-welcome](https://img.shields.io/badge/PRs-welcome-green?style=plastic)
![g++-10](https://img.shields.io/badge/g++-≥10-blue?style=plastic)
![gfortran-10](https://img.shields.io/badge/gfortran-≥10-blueviolet?style=plastic)

# Qsyn: An end-to-end quantum compilation framework
<!-- ![example branch parameter](https://github.com/ric2k1/qsyn/actions/workflows/build-and-test.yml/badge.svg)
 -->

## Introduction
Qsyn is a C++ based growing software system for synthesis, optimization and verification of quantum circuits appearing in quantum computers. Qsyn combines scalable quantum circuits optimization by implementing ZX-Calculus and technology mapping.

Qsyn provides an experimental implementation of optimization algorithms and a programming envirnoment for simulation or building similar applications. Future development will focus on enhancing the algorithms ,visualization of ZX-Graphs and implementation of lattice surgery for error correction codes.

## Getting Start
### Installation
```shell!
git clone https://github.com/ric2k1/qsyn.git
cd qsyn
```

### Compilation
`qsyn` requires at least `g++-10` and `gfortran-10` to compile. The two compilers should also be on the same version.

To compile, first install the dependencies by

```shell!
./configure.sh
```
This script will check for lacking dependencies and install it automatically. Then, run
```shell!
make -j8
```
To build up the executable. 

### Run

* To run in command-line mode:
    ```shell!
    ./qsyn
    ```

* To run the demo program, give it a file contains cmds. For example:
    ```shell!
    ❯ ./qsyn < tests/demo/demo/dof/tof_3.dof
    qsyn> verb 0
    Note: verbose level is set to 0

    qsyn> zxm -on 
    ZXMODE turn ON!

    qsyn> zxgread benchmark/zx/tof3.zx

    qsyn> zxgs -freduce

    qsyn> zxgp
    Graph 0
    Inputs:        3
    Outputs:       3
    Vertices:      17
    Edges:         19

    qsyn> q -f
    ```

* The same result can be produced by running in the command-line mode:
    ```shell!
    ❯ ./qsyn
    qsyn> dofile tests/demo/demo/dof/tof_3.dof

    qsyn> verb 0
    Note: verbose level is set to 0

    qsyn> zxm -on 
    ZXMODE turn ON!

    qsyn> zxgread benchmark/zx/tof3.zx

    qsyn> zxgs -freduce

    qsyn> zxgp
    Graph 0
    Inputs:        3
    Outputs:       3
    Vertices:      17
    Edges:         19

    qsyn> q -f
    ```



### Testing
There are two types of tests:
1. DOFILEs, which automatically run a sequence of commands;
2. unit tests, which checks the validity of selected functions.

DOFILEs are located under `tests/<DATE>/<TEST_PACKAGE>/testcases/`. 

To run a DOFILE and compare the result to the reference, run
```shell!
./DOFILE.sh <path/to/test> -d
```
To update the reference to a dofile, run
```shell!
./DOFILE.sh <path/to/test> -up
```
To compile unit test programs, run
```shell!
make test -j8
```
Then, run the test by 
```shell!
./tests/bin/tests -r compact
```
You may also perform all DOFILE- and unit-tests by running
```bash!
./RUN_ALL_TEST.sh
```


## Commands List

### Info
| Cmd          | Description                         			| Options     |
| --------     | --------                            			| --------    |
| DOfile       | execute the commands in the dofile  			|             |
| FORMAT       | set format level (0: none, 1: all)  			|             |
| HELp         | print this help message             			|             |
| HIStory      | print command history               			|             |
| Quit         | quit the execution                  			|             |
| SEED         | fix the seed                        			|             |
| USAGE        | report the runtime and/or memory usage         |             |
| VERbose      | set verbose level (0-9)                        |             |



### QCir

| Cmd          | Description                         					| Options     |
| --------     | --------                            					| --------    |
| QCBAdd       | add qubit(s)					                		|             |
| QCBDelete    | delete an empty qubit					        		|             |
| QCCPrint     | print quanutm circuit					        		|             |
| QCCRead      | read a circuit and construct corresponding netlist		|             |
| QCCWrite     | write QASM file					                    |             |
| QCGAdd       | add quantum gate					                    |             |
| QCGDelete    | delete quantum gate				                    |             |
| QCGPrint     | print quantum gate information				            |             |
| QCTSMapping  | mapping to tensor from quantum circuit				    |             |
| QCZXMapping  | mapping to ZX-graph from quantum circuit				|             |



### Graph

| Cmd           | Description                         								| Options     |
| --------      | --------                            								| --------    |
| ZXCHeckout    | chec kout to Graph <id> in ZXGraphMgr                				|             |
| ZXCOMpose     | compose a ZX-graph				                                |             |
| ZXCOPy        | copy a ZX-graph				                                    |             |
| ZXGASsign     | assign an input/output vertex to specific qubit					|             |
| ZXGEdit       | edit ZX-graph    			                                        |             |
| ZXGPrint      | print info in ZX-graph    			                            |             |
| ZXGRead       | read a ZXGraph    			                                    |             |
| ZXGSimp       | perform simplification strategies for ZX-graph        			|             |
| ZXGTest       | test ZX-graph structures and functions	    			        |             |
| ZXGTRaverse   | traverse ZX-graph and update topological order of vertices	    |             |
| ZXGTSMapping  | mapping to tensor from ZX-Graph    			                    |             |
| ZXGWrite      | write ZXFile    			                                        |             |
| ZXMode        | check out to ZX-graph mode    			                        |             |
| ZXNew         | new ZX-graph to ZXGraphMgr        			                    |             |
| ZXPrint       | print info in ZXGraphMgr	    			                        |             |
| ZXRemove      | remove ZX-graph from ZXGraphMgr				                    |             |
| ZXTensor      | tensor a ZX-graph				                                    |             |



### Tensor
| Cmd          | Description                                         | Options     |
| --------     | --------                                            | --------    |
| TSEQuiv	   | compare the equivalency of two stored tensors	     |             |
| TSPrint	   | Print information about stored tensors	             |             |
| TSReset	   | Reset the tensor manager	                         |             |

