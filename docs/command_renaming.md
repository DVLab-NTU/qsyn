# Command Renaming

This document lists the commands that have been renamed in the new version of Qsyn.

## CLI Common Commands

| Original Command | New Command | Synopsis             | Notes                         |
| :--------------- | :---------- | :------------------- | :---------------------------- |
| n/a              | alias       | set alias to command |                               |
| n/a              | set         | set variables        |                               |
| help             | help        | show help            |                               |
| qquit            | quit        | quit qsyn            |                               |
| history          | history     | show history         | added output/clear flags      |
| dofile           | source      | run script           |                               |
| usage            | usage       | show usage           |                               |
| seed             | n/a         | set seed             | left to individual commands\* |
| clear            | clear       | clear terminal       |                               |
| verbose          | logger      | set logger level     | migrated to logger system     |

\* It is not a good nor a practical idea to tie every rand-gen to the same random device.

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

| Original Command | New Command         | Synopsis                  | Notes                |
| :--------------- | :------------------ | :------------------------ | :------------------- |
| qc2zx            | convert qcir zx     | convert QCir to ZXGraph   | also alised to qc2zx |
| qc2ts            | convert qcir tensor | convert QCir to tensor    | also alised to qc2ts |
| zx2ts            | convert zx tensor   | convert ZXGraph to tensor | also alised to zx2ts |
| zx2qc            | convert zx qcir     | convert ZXGraph to QCir   | also alised to zx2qc |

## Duostra Commands

| Original Command | New Command        | Synopsis                                          | Notes |
| :--------------- | :----------------- | :------------------------------------------------ | :---- |
| duostra          | duostra            | run duostra mapper to a quantum circuit           |       |
| duoset           | duostra config ... | set duostra config                                |       |
| duoprint         | duostra config     | print duostra config                              |       |
| mpequiv          | map-equiv          | check if logical and physical QCir are equivalent |       |

## Extractor Commands

| Original Command    | New Command        | Synopsis                          | Notes               |
| :------------------ | :----------------- | :-------------------------------- | :------------------ |
| zx2qc               | extract -full      | extract ZXGraph to QCir           | zx2qc becomes alias |
| extract             | extract            | extract a part of ZXGraph to QCir |                     |
| extprint --settings | extract config     | print extract config              |                     |
| extprint            | extract print      | print extractor status            |                     |
| extset              | extract config ... | set extract config                |                     |

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
| qccoptimize      | qcir optimize      | optimize QCir                   |                                 |
| qcreset          | qcir delete --all  | delete all QCir                 |                                 |

## Tensor Commands

| Original Command | New Command         | Synopsis                       | Notes |
| :--------------- | :------------------ | :----------------------------- | :---- |
| tsprint          | tensor              | print tensor                   |       |
| tsprint -l       | tensor list         | list tensor                    |       |
| n/a              | tensor delete       | delete tensor                  |       |
| tsreset          | tensor delete --all | delete all tensor              |       |
| tscheckout       | tensor checkout     | checkout tensor                |       |
| tscopy           | tensor copy         | copy tensor                    |       |
| tsequiv          | tensor equiv        | check if tensor are equivalent |       |
| tstprint         | tensor print        | print tensor in focus          |       |
| tsadjoint        | tensor adjoint      | perform adjoint                |       |

## ZXGraph Commands

| Original Command   | New Command      | Synopsis                          | Notes                                      |
| :----------------- | :--------------- | :-------------------------------- | :----------------------------------------- |
| zxcheckout         | zx checkout      | checkout ZXGraph                  |                                            |
| zxprint            | zx               | print ZXGraph                     |                                            |
| zxprint -l         | zx list          | list ZXGraph                      |                                            |
| zxgprint           | zx print         | print ZXGraph in focus            |                                            |
| zxreset            | zx delete --all  | delete all ZXGraph                |                                            |
| zxnew              | zx new           | new ZXGraph                       |                                            |
| zxdelete           | zx delete        | delete ZXGraph                    |                                            |
| zxcopy             | zx copy          | copy ZXGraph                      |                                            |
| zxcompose          | zx compose       | compose ZXGraph                   |                                            |
| zxtensor           | zx direct        | perform direct (tensor) product   |                                            |
| zxgtest            | zx test          | test ZXGraph attributes           |                                            |
| zxgedit -addvertex | zx vertex add    | add vertex to ZXGraph             |                                            |
| zxgedit -addedge   | zx edge add      | add edge to ZXGraph               |                                            |
| zxgedit -rmvertex  | zx vertex remove | remove vertex from ZXGraph        |                                            |
| zxgedit -rmedge    | zx edge remove   | remove edge from ZXGraph          |                                            |
| zxgassign          | zx assign        | assign vertex to a boundary qubit | may be moved under zx vertex in the future |
| zxgadjoint         | zx adjoint       | perform adjoint                   |                                            |
| zxgdraw            | zx draw          | draw ZXGraph                      | deprecated CLI drawing                     |
| zxgread            | zx read          | read ZXGraph from file            |                                            |
| zxgwrite           | zx write         | write ZXGraph to file             |                                            |
| zxgsimp [routine]  | zx optimize ...  | optimize ZXGraph                  |                                            |
| zxgsimp [rule]     | zx rule ...      | apply rule                        |                                            |
| zxggflow           | zx gflow         | perform gflow                     |                                            |
