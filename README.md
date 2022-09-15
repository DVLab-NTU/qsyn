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
$ make < mac | linux18 > 
```
depending on your OS. Finally, 
```
$ make -j8
```
To build up the executable. 

## Testing
To compile test programs, run
```
$ make test -j8
```
All the test programs are soft-linked into `tests/`. You run individual test programs from there, or use 
```
$ ./RUN_ALL_TEST.sh
```
to run all tests at once. You may add new test programs in `src/tests`.

