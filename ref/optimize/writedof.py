import sys
import os

filename = sys.argv[1]

with open("largecase.dof", "w") as f:
    f.write("color off\n")
    f.write("verbose 0\n")
    f.write("qccr "+ sys.argv[1]+ '\n')
    f.write("qc2zx\n")
    f.write("zxgsimp -fr\n")
    f.write("zx2qc\n")
    f.write("usage\n")
    f.write("optimize\n")
    f.write("usage\n")
    f.write("qccp\n")
    f.write("qc2zx\n")
    f.write("zxgadj\n")
    f.write("zxcomp 0\n")
    f.write("zxgs -fr\n")
    f.write("zxgp -a\n")
    f.write("zxgt -id\n")
    f.write("qq -f\n")


'''
color off
verbose 0
qccread benchmark/benchmark_SABRE/large/{}.qasm
qc2zx
zxgsimp -fr
zx2qc
optimize
qc2zx  
zxgadj
zxcomp 0 
zxgs -fr  
zxgp -a       
zxgt -id 
qq -f

'''