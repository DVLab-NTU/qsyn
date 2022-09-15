# qsyn: The tool of quantum circuit compilation. 
## Support functions:
* ZX calculus optimization
* Circuit extraction
* Logical circuit optimization
* Physical-aware Optimization
* Lattice Surgery

## Support Cmds
* [qcir](https://www.notion.so/Circuit-Data-Structure-dcf1018d7ac14ff18edd83631fd3fd71#0aa0ceb16d28496d9d5b5eb6d747850b)

## Compilation
`qsyn` requires at least `g++-10` and `gfortran-10` to compile. The two compilers should also be on the same version.

To compile, first install the dependencies by
```
$ ./configure.sh
```
This script will check for lacking dependencies and install it automatically. Then, run
```
$ make -j8
```
To build up the executable. 

## Testing
There are two types of tests:
1. DOFILEs, which automatically run a sequence of commands;
2. unit tests, which checks the validity of selected functions.

DOFILEs are located under `tests/<DATE>/<TEST_PACKAGE>/testcases/`. 

To run a DOFILE and compare the result to the reference, run
```
$ ./DOFILE.sh -d <path/to/test>
```
To update the reference to a dofile, run
```
$ ./DOFILE.sh -u <path/to/test>
```
To compile unit test programs, run
```
$ make test -j8
```
Then, run the test by 
```
./tests/bin/tests -r compact
```
You may also perform all DOFILE- and unit-tests by running
```
$ ./RUN_ALL_TEST.sh
```

