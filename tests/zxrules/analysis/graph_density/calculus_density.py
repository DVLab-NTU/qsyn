from parse import *
import sys
from collections import OrderedDict




def main():
    filename = sys.argv[1]
    circuit = sys.argv[2]
    print("Circuit: "+ circuit.rsplit('/', 1)[1])
    flag = False
    i = True
    neighbor_num_init = {}
    neighbor_num_simp = {}

    with open(filename, 'r') as f:
        lines = f.readlines()

    for line in lines:
        if line == "\n":
            pass
        elif flag:
            if line.startswith("qsyn>"):
                flag = False
                #print('Count done')
                i = False
            else:
                neighbor_num = int(parse("ID:{}#Neighbors:{}({}", line)[1])
                if i:
                    addnum2dict(neighbor_num_init, neighbor_num)
                else:
                    addnum2dict(neighbor_num_simp, neighbor_num)
                #print("neighbor is %d\n" %neighbor_num)
        else:
            if line == "qsyn> zxgp -Q\n":
                flag = True
                #print("Flag is true")
            else:
                pass
    print("Init:")
    #print(neighbor_num_init)
    print(OrderedDict(sorted(neighbor_num_init.items())))
    print("Average= %.2f" %average(neighbor_num_init))
    print("After full reduce")
    #print(neighbor_num_simp)
    print(OrderedDict(sorted(neighbor_num_simp.items())))
    print("Average= %.2f\n\n" %average(neighbor_num_simp))

        
def addnum2dict(dict, num):
    if num in dict:
        dict[num] += 1
    else:
        dict[num] = 1

def average(dict):
    sum = 0
    n = 0
    for key in dict:
        sum += key*dict[key]
        n += dict[key]
    return round(sum/n, 2)


if __name__ == '__main__':
    main()

