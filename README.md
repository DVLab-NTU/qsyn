# qsyn: The tool of quantum circuit compilation. 
## Support functions:
* ZX calculus optimization
* Circuit extraction
* Logical circuit optimization
* Physical-aware Optimization
* Lattice Surgery

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

