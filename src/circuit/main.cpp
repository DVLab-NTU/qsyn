// * /////////////////////////////////////////////////////////////////////////////////////////////
// *
// * FileName     [ main.cpp ]
// * PackageName  [ src/circuit ]
// * Synopsis     [ Temporary parser, needed to be intergrated to the framework ]
// * Author       [ Chin-Yi Cheng ]
// * Copyright    [ 2022 6 ]
// *
// * /////////////////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
using namespace std;

void addGate(size_t id, string type, vector<size_t> pins)
{
    // Do what ever you want
    cout << "Gate " << id << ": " << type << " \t"
         << " Qubit: ";
    for (size_t i = 0; i < pins.size(); i++)
    {
        cout << pins[i] << " ";
    }
    cout << endl;
}
void parser(fstream &qasm_file)
{
    string str;
    for (int i = 0; i < 6; i++)
    {
        // OPENQASM 2.0;
        // include "qelib1.inc";
        // qreg q[int];
        qasm_file >> str;
    }
    size_t gate_id = 0;
    const size_t num_qubits =
        stoi(str.substr(str.find("[") + 1, str.size() - str.find("[") - 3));

    vector<string> single_list{"x", "sx", "s", "rz", "i", "h", "t", "tdg"};

    while (qasm_file >> str)
    {
        string space_delimiter = " ";
        string type = str.substr(0, str.find(" "));
        type = str.substr(0, str.find("("));
        string is_CX = type.substr(0, 2);
        string is_CRZ = type.substr(0, 3);

        if (is_CX != "cx" && is_CRZ != "crz")
        {
            if (find(begin(single_list), end(single_list), type) !=
                end(single_list))
            {
                qasm_file >> str;
                string singleQubitId = str.substr(2, str.size() - 4);
                size_t q = stoul(singleQubitId);
                vector<size_t> pin_id;
                pin_id.push_back(q);
                addGate(gate_id, type, pin_id);
                gate_id++;
            }
            else
            {
                if (type != "creg" && type != "qreg")
                {
                    cerr << "Unseen Gate " << type << endl;
                    exit(0);
                }
                else
                {
                    qasm_file >> str;
                }
            }
        }
        else
        {
            qasm_file >> str;
            string delimiter = ",";
            string token = str.substr(0, str.find(delimiter));
            string qubit_id = token.substr(2, token.size() - 3);
            size_t q1 = stoul(qubit_id);
            token = str.substr(str.find(delimiter) + 1,
                               str.size() - str.find(delimiter) - 2);
            qubit_id = token.substr(2, token.size() - 3);
            size_t q2 = stoul(qubit_id);
            vector<size_t> pin_id;
            pin_id.push_back(q1);
            pin_id.push_back(q2);
            addGate(gate_id, type, pin_id);
            gate_id++;
        }
    }
}

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
    parser(input_qasm_file);
    return 0;
}
