import sys
import os

filename = sys.argv[1]

with open("countGates.dof", "w") as f:
    f.write("color off\n")
    f.write("qccr "+ sys.argv[1]+ '\n')
    f.write("qccp -a\n")
    # f.write("zxgsimp -fr\n")
    # f.write("zxgwrite zxgraph/"+sys.argv[1].split("/")[-1].split(".")[0]+".zx -c\n")
    f.write("qq -f\n")
