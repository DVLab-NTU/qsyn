
![license](https://img.shields.io/github/license/ric2k1/qsyn?style=plastic)
![stars](https://img.shields.io/github/stars/ric2k1/qsyn?style=plastic)
![contributors](https://img.shields.io/github/contributors/ric2k1/qsyn?style=plastic)
![pr-welcome](https://img.shields.io/badge/PRs-welcome-green?style=plastic)
![g++-10](https://img.shields.io/badge/g++-≥10-blue?style=plastic)
![gfortran-10](https://img.shields.io/badge/gfortran-≥10-blueviolet?style=plastic)

# Qsyn: An End-to-End Quantum Program Compilation Framework
<!-- ![example branch parameter](https://github.com/ric2k1/qsyn/actions/workflows/build-and-test.yml/badge.svg)
 -->
 ![](https://i.imgur.com/x2lKZXb.png)

## Introduction
Qsyn is a C++ based growing software system for synthesis, optimization and verification of quantum circuits appearing in quantum computers. Qsyn combines scalable quantum circuits optimization by implementing ZX-Calculus and qubit mapping.

Qsyn provides an experimental implementation of optimization algorithms and a programming environment for simulation or building similar applications. Future development will focus on enhancing the algorithms, visualization of ZX-Graphs and implementation of lattice surgery for error correction codes.

## Getting Started
### Installation
```shell!
git clone https://github.com/ric2k1/qsyn.git
cd qsyn
```

### Compilation
`Qsyn` requires at least `g++-10` and `gfortran-10` to compile. The two compilers should also be on the same version.

1. Install the dependencies by `configure.sh`, which will check for lacking dependencies and install them automatically. 

	```shell!
	./configure.sh
	```
2. Run `Makefile` using the command below builds up the executable.
	```shell!
	make -j8
	```
3. If the compilation process ends successfully, you will get
    ```shell!
    Checking main...
    > compiling: main.cpp
    > building qsyn...
    ~/qsyn$ _
    ```
4. If you want to delete all intermediate files created in the compilation process, please type
    ```shell!
	make clean
	```

### Run

* After successful compilation, you can run `Qsyn` interactively by
    
   ```shell!
    ❯ ./qsyn
    DV Lab, NTUEE, Qsyn 0.3.0
    qsyn> 
   ```


* To run the demo program, you can provide a file containing commands. For example,
    ```shell!
    ❯ ./qsyn < tests/demo/demo/dof/tof_3.dof
    DV Lab, NTUEE, Qsyn 0.3.0
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
    DV Lab, NTUEE, Qsyn 0.3.0
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
There are two types of testing approaches:
1. DOFILEs, which automatically run a sequence of commands. (Located under `tests/<section>/<subsection>/dof/`)

    * To run a DOFILE and compare the result to the reference, type
		```shell!
		./DOFILE.sh <path/to/test> -d
		```
    * To update the reference to a dofile, type
		```shell!
		./DOFILE.sh <path/to/test> -up
		```
    You may also run all DOFILEs by running
    ```bash!
    ./RUN_ALL_TEST.sh
    ```
2. Unit tests, which checks the validity of selected functions.
	
    * To compile unit test programs, type
		```shell!
		make test -j8
		```
	* Then, run the test by 
		```shell!
		./tests/bin/tests -r compact
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
| QC2TS        | convert the quantum circuit to tensor				    |             |
| QC2ZX        | convert the quantum circuit to ZX-graph				|             |
| QCBAdd       | add qubit(s)					                		|             |
| QCBDelete    | delete an empty qubit					        		|             |
| QCCPrint     | print quanutm circuit					        		|             |
| QCCRead      | read a circuit and construct corresponding netlist		|             |
| QCCWrite     | write QASM file					                    |             |
| QCGAdd       | add quantum gate					                    |             |
| QCGDelete    | delete quantum gate				                    |             |
| QCGPrint     | print quantum gate information				            |             |




### Graph

| Cmd           | Description                         								| Options     |
| --------      | --------                            								| --------    |
| ZX2TS         | convert the ZX-graph to tensor    			                    |             |
| ZXCHeckout    | checkout to Graph <id> in ZXGraphMgr                				|             |
| ZXCOMpose     | compose a ZX-graph				                                |             |
| ZXCOPy        | copy a ZX-graph				                                    |             |
| ZXGASsign     | assign an input/output vertex to specific qubit					|             |
| ZXGADJoint    | adjoint the current ZXGraph | |
| ZXGEdit       | edit ZX-graph    			                                        |             |
| ZXGPrint      | print info in ZX-graph    			                            |             |
| ZXGRead       | read a ZXGraph    			                                    |             |
| ZXGSimp       | perform simplification strategies for ZX-graph        			|             |
| ZXGTest       | test ZX-graph structures and functions	    			        |             |
| ZXGTRaverse   | traverse ZX-graph and update topological order of vertices	    |             |
| ZXGWrite      | write ZXFile    			                                        |             |
| ZXMode        | check out to ZX-graph mode    			                        |             |
| ZXNew         | new ZX-graph to ZXGraphMgr        			                     |             |
| ZXPrint       | print info in ZXGraphMgr	    			                        |             |
| ZXRemove      | remove ZX-graph from ZXGraphMgr				                    |             |
| ZXTensor      | tensor a ZX-graph				                                    |             |



### Tensor
| Cmd          | Description                                         | Options     |
| --------     | --------                                            | --------    |
| TSADJoint    | adjoint the specified tensor | |
| TSEQuiv	   | compare the equivalency of two stored tensors	     |             |
| TSPrint	   | print information about stored tensors	             |             |
| TSReset	   | reset the tensor manager	                         |             |

