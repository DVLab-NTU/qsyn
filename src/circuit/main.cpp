// * /////////////////////////////////////////////////////////////////////////////////////////////
// *
// * FileName     [ main.cpp ]
// * PackageName  [ src/circuit ]
// * Synopsis     [ Temporary parser, needed to be intergrated to the framework ]
// * Author       [ Chin-Yi Cheng ]
// * Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
// *
// * /////////////////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include "qcirMgr.h"
using namespace std;

int main(int argc, char **argv)
{
    fstream input_qasm_file;
    if (argc == 2)
    {
        input_qasm_file.open(argv[1], ios::in);
        if (!input_qasm_file)
        {
            cerr << "Cannot open the input file \"" << argv[1]
                 << "\". The program will be terminated..." << endl;
            exit(1);
        }
    }
    else
    {
        cerr << "Usage: ./cir <input qasm>" << endl;
        exit(1);
    }
    QCirMgr *qCircuit = new QCirMgr();
    qCircuit->parseQASM(input_qasm_file);
    qCircuit->printGates();
    return 0;
}
