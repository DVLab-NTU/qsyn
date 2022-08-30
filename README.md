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
`qsyn` supports macOS, linux16 and linux18. To compile, first run
```
$ make < mac | linux16 | linux18 > 
```
depending on your OS. Then, 
```
$ make
```
To build up the executable. 

## Testing
To compile test programs, run
```
$ make test
```
All the test programs are soft-linked into `tests/`. You run individual test programs from there, or use 
```
$ ./RUN_ALL_TEST.sh
```
to run all tests at once. You may add new test programs in `src/tests`.

