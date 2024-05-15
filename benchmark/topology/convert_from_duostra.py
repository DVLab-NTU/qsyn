import os
def list2string(list):
    list = [str(i) for i in list]
    result = ', '.join(list)
    result = "[" + result + "]"
    return result

def device2topologic(devicepath, outputdir):
    coupling_map = []
    sg_error = []
    name = devicepath.split("/")[-1]
    name = name.split(".")[0]
    with open(devicepath, 'r') as f:
        f.readline()
        lines = f.readlines()
        for line in lines:
            new_qubit = []
            list = line.split(' ')
            list[-1] = list[-1][:-1]
            sg_error.append(0)
            # list[0] is in order
            # number = list[1]
            for neb in list[2:]:
                # print(neb)
                new_qubit.append(int(neb))
            coupling_map.append(new_qubit)

    with open("/".join([outputdir, name+".layout"]), 'w') as g:
        g.writelines("NAME: {}\n".format(name))
        g.writelines("QUBITNUM: {}\n".format(len(coupling_map)))
        g.writelines("GATESET: {x, rz, h, id, sx, cnot}\n")
        g.write("COUPLINGMAP: ")
        g.writelines(list2string(coupling_map))
        g.writelines("\n")
        g.write("SGERROR: ")
        g.writelines(list2string(sg_error))
        g.writelines("\n")
        g.write("SGTIME: ")
        g.writelines(list2string(sg_error))
        g.writelines("\n")
        g.write("CNOTERROR: ")
        g.writelines(list2string(coupling_map))
        g.writelines("\n")
        g.write("CNOTTIME: ")
        g.writelines(list2string(coupling_map))
        g.writelines("\n")
        # # g.writelines(coupling_map)
        # pass
    # print(coupling_map)

root = "/home/chinyi0523/QFT-mapping/device/"
for file in os.listdir(root):
    if file.split(".")[-1] == "txt":
        print("/".join([root, file]), ".")
        device2topologic("/".join([root, file]), ".")

# device2topologic("/home/arttr1521/git/qft-mapping/device/kolkata_27.txt", "kolkata_27.layout")
    # with open(outputpath, 'w') as g:
    #     g.writelines("NAME: \n")
    #     g.writelines("QUBITNUM: \n")
    #     g.writelines("GATESET: {x, rz, h, id, sx, cnot}\n")
    #     g.writelines("COUPLINGMAP:\n")
    #     g.write(coupling_map)
    #     # g.writelines(coupling_map)
    #     pass
