# Getting Start with Qsyn
###### tags: `qsyn`

## How to start running Qsyn ?
### Installation
```shell!
git clone https://github.com/ric2k1/qsyn.lab.git
cd qsyn.lab
```

### Compilation
```shell!
make -j16
```

### Run
```shell!
./qsyn
```

> Read ./README.md for more information if you have any question.

## Command Briefly Introduction

* Information
    * help: see all commands and briefly introduction
    * history: print command history
    * usage: report the runtime and/or memory usage
    * qquit: quit the exection
* Settings
    * color: toggle colored printing in command line (default: true)
    * verbose: set verbosity levle to 0-9 (default: 3)
    * seed: set the random seed
* Device Topology
    * Commands start with **DT**
* DuosTra
    * Commands start with **DUO**
* EXTraction
    * Commands start with **EXT**
* Quantum Circuit
    * Commands start with **QC**
* ZX-graph
    * Commands start with **ZX**
* TenSor
    * Commands start with **TS**

> You can type the first several characters of the command and press **`Tab`** to see more information for the corresponding command.

## A Basic Example

> See /examples/basic_cmd.dof for full commands

1. Read a QASM file of QFT_3 circuit and print some information
```
qccread ./benchmark/qft/qft_3.qasm 
qccprint -s
qccprint -q
```

![](https://hackmd.io/_uploads/BJccpspB3.png)


2. Transform the quantum circuit to ZX-graph
```
qc2zx
```

3. Optimize ZX-graph using `full_reduce` and print infos for ZX-graph before and after the optimization

```
zxgprint -s
zxcopy
zxgdraw ./examples/pdf/qft_3-before.pdf
zxgwrite ./examples/zx-format/qft_3-before.zx
zxgsimp -freduce
zxgprint -s
zxgdraw ./examples/pdf/qft_3-after.pdf
zxgwrite ./examples/zx-format/qft_3-after.zx
```

![](https://hackmd.io/_uploads/ByMqRjarh.png)


4. Transform the optimized ZX-graph back to quantum circuit and write it to a new QASM file

```
zxcopy
zx2qc
qccprint -s
qccprint -q
qccwrite ./examples/qasm/qft_3-after.qasm
```

5. Transform the original QASM ans the optimized one we just generated to tensor to check if they are equivalent

```
qccheckout 0 // The original one
qc2ts
qccheckout 1 // The optimized one
qc2ts
tsequiv 0 1
```

![](https://hackmd.io/_uploads/BJ8nRi6H3.png)


6. Adjoint the original ZX-graph and compose it with the optimized one and `full_reduce` the new ZX-graph again to check if it is identity ($AA^{T} = I$)

```
zxcheckout 1
zxgadjoint
zxcompose 0
zxgsimp -freduce
zxgp -s
zxgp -v
```
![](https://hackmd.io/_uploads/rkx-1nprh.png)

7. Quit the exection
```
qq -f
```


## Try yourself
1. Change verbosity from 0-9 and see the printing information in command line
2. Use `qcprint`, `zxprint`, and `tsprint` to see the information of `QCirMgr`, `ZXGraphMgr` and `TSMgr`
3. Use different simplification rules in `zxgsimp` cmd and see the difference before and after those rules.
![](https://hackmd.io/_uploads/ryp2lnTS3.png)
