# Command Renaming

## CLI Common Commands

| Original Command | New Command | Synopsis             | Notes                                       |
| :--------------- | :---------- | :------------------- | :------------------------------------------ |
| n/a              | alias       | set alias to command |                                             |
| n/a              | set         | set variables        |                                             |
| help             | help        | show help            | consider repurpose                          |
| qquit            | quit        | quit qsyn            | renamed because alias system is implemented |
| history          | history     | show history         | consider add filter, dump functions         |
| dofile           | source      | run script           | also aliased to dofile                      |
| usage            | usage       | show usage           | consider add current usage                  |
| seed             | seed        | set seed             | I don't know if this actually makes sense?  |
| clear            | clear       | clear terminal       |                                             |
| logger           | logger      | set logger level     |                                             |

## Device Commands

| Original Command | New Command     | Synopsis                         | Notes |
| :--------------- | :-------------- | :------------------------------- | :---- |
| dtprint          | device          | print # device & current focus   |       |
| dtprint -l       | device list     | print info of each device        |       |
| dtreset          | device clear    | clear the device list            |       |
| dtdelete         | device delete   | delete a device                  |       |
| dtcheckout       | device checkout | checkout a device                |       |
| dtgprint         | device print    | print the current focused device |       |

## Conversion Commands

| Original Command | New Command | Synopsis                  | Notes            |
| :--------------- | :---------- | :------------------------ | :--------------- |
| qc2zx            | qcir2zx     | convert QCir to ZXGraph   | aliased as qc2zx |
| qc2ts            | qcir2tensor | convert QCir to tensor    | aliased as qc2ts |
| zx2ts            | zx2tensor   | convert ZXGraph to tensor | aliased as zx2ts |

## Duostra Commands

| Original Command | New Command        | Synopsis                                          | Notes |
| :--------------- | :----------------- | :------------------------------------------------ | :---- |
| duostra          | duostra            | run duostra mapper to a quantum circuit           |       |
| duoset           | duostra config ... | set duostra config                                |       |
| duoprint         | duostra config     | print duostra config                              |       |
| mpequiv          | qcir mapequiv      | check if logical and physical QCir are equivalent |       |

## Extractor Commands

| Original Command | New Command        | Synopsis                          | Notes               |
| :--------------- | :----------------- | :-------------------------------- | :------------------ |
| zx2qc            | extract -full      | extract ZXGraph to QCir           | zx2qc becomes alias |
| extract          | extract            | extract a part of ZXGraph to QCir |                     |
| extprint         | extract config     | print extract config              |                     |
| extset           | extract config ... | set extract config                |                     |

## QCir Commands

| Original Command | New Command        | Synopsis                        | Notes                           |
| :--------------- | :----------------- | :------------------------------ | :------------------------------ |
| qcprint          | qcir               | print QCir                      |                                 |
| qcprint -l       | qcir list          | list QCir                       |                                 |
| qccprint         | qcir print         | print QCir in focus             |                                 |
| qcreset          | qcir clear         | clear QCir                      |                                 |
| qcnew            | qcir new           | new QCir                        |                                 |
| qcdelete         | qcir delete        | delete QCir                     |                                 |
| qccheckout       | qcir checkout      | checkout QCir                   |                                 |
| qccopy           | qcir copy          | copy QCir                       |                                 |
| qccompose        | qcir compose       | compose QCir                    |                                 |
| qctensor         | qcir direct        | perform direct (tensor) product |                                 |
| qcset            | qcir config ...    | set QCir config                 |                                 |
| qcprint -s       | qcir config        | print QCir config               |                                 |
| qccread          | qcir read          | read QCir from file             |                                 |
| qccwrite         | qcir write         | write QCir to file              |                                 |
| qccdraw          | qcir draw          | draw QCir                       | move qccd -d text to qcir print |
| qccdraw -d text  | qcir print --fancy | draw QCir in text format        |                                 |
| qcba             | qcir qubit add     | add qubit to QCir               |                                 |
| qcbd             | qcir qubit remove  | remove qubit from QCir          |                                 |
| qcga             | qcir gate add      | add gate to QCir                |                                 |
| qcgd             | qcir gate remove   | remove gate from QCir           |                                 |
| qcgprint         | qcir gate print    | print QCir gate                 |                                 |
| n/a              | qcir adjoint       | perform adjoint                 |                                 |

## QCir Optimizer Commands

| Original Command | New Command   | Synopsis      | Notes |
| :--------------- | :------------ | :------------ | :---- |
| qccoptimize      | qcir optimize | optimize QCir |       |

## Tensor Commands

| Original Command | New Command     | Synopsis                       | Notes |
| :--------------- | :-------------- | :----------------------------- | :---- |
| tsprint          | tensor          | print tensor                   |       |
| tsprint -l       | tensor list     | list tensor                    |       |
| tsreset          | tensor clear    | clear tensor                   |       |
| n/a              | tensor delete   | delete tensor                  |       |
| tscheckout       | tensor checkout | checkout tensor                |       |
| tscopy           | tensor copy     | copy tensor                    |       |
| tsequiv          | tensor equiv    | check if tensor are equivalent |       |
| tstprint         | tensor print    | print tensor in focus          |       |
| tsadjoint        | tensor adjoint  | perform adjoint                |       |

## ZXGraph Commands

| Original Command   | New Command           | Synopsis                          | Notes                  |
| :----------------- | :-------------------- | :-------------------------------- | :--------------------- |
| zxcheckout         | zxgraph checkout      | checkout ZXGraph                  |                        |
| zxprint            | zxgraph               | print ZXGraph                     |                        |
| zxprint -l         | zxgraph list          | list ZXGraph                      |                        |
| zxgprint           | zxgraph print         | print ZXGraph in focus            |                        |
| zxreset            | zxgraph clear         | clear ZXGraph                     |                        |
| zxnew              | zxgraph new           | new ZXGraph                       |                        |
| zxdelete           | zxgraph delete        | delete ZXGraph                    |                        |
| zxcopy             | zxgraph copy          | copy ZXGraph                      |                        |
| zxcompose          | zxgraph compose       | compose ZXGraph                   |                        |
| zxtensor           | zxgraph direct        | perform direct (tensor) product   |                        |
| zxgtest            | zxgraph test          | test ZXGraph attributes           |                        |
| zxgedit -addvertex | zxgraph vertex add    | add vertex to ZXGraph             |                        |
| zxgedit -addedge   | zxgraph edge add      | add edge to ZXGraph               |                        |
| zxgedit -rmvertex  | zxgraph vertex remove | remove vertex from ZXGraph        |                        |
| zxgedit -rmedge    | zxgraph edge remove   | remove edge from ZXGraph          |                        |
| zxgassign          | zxgraph assign        | assign vertex to a boundary qubit | change API?            |
| zxgadjoint         | zxgraph adjoint       | perform adjoint                   |                        |
| zxgdraw            | zxgraph draw          | draw ZXGraph                      | deprecate CLI drawing? |
| zxgread            | zxgraph read          | read ZXGraph from file            |                        |
| zxgwrite           | zxgraph write         | write ZXGraph to file             |                        |

## ZXGraph Optimizer Commands

| Original Command  | New Command          | Synopsis         | Notes                         |
| :---------------- | :------------------- | :--------------- | :---------------------------- |
| zxgsimp [routine] | zxgraph optimize ... | optimize ZXGraph |                               |
| zxgsimp [rule]    | zxgraph rule ...     | apply rule       | add match and selective apply |

## ZXGraph GFlow Commands

| Original Command | New Command   | Synopsis      | Notes |
| :--------------- | :------------ | :------------ | :---- |
| zxgflow          | zxgraph gflow | perform gflow |       |
