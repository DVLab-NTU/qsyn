/****************************************************************************
  FileName     [ qcirMgr.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir manager functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <vector>
#include <string>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <algorithm>
#include "qcirMgr.h"

using namespace std;
QCirMgr *qCirMgr = 0;

void QCirMgr::printSummary() const
{
    cout << "Follow QASM file (Topological order)" << endl;
    for (size_t i = 0; i < _qgate.size(); i++)
    {
        _qgate[i]->printGate();
    }
}
void QCirMgr::printQubits() const
{
    for (size_t i = 0; i < _qubits.size(); i++)
        _qubits[i]->printBitLine();
}
void QCirMgr::addAncilla(size_t num)
{
    for (size_t i = 0; i < num; i++)
    {
        QCirQubit* temp = new QCirQubit(_qubits.size());
        _qubits.push_back(temp);
    }
}
void QCirMgr::removeAncilla(size_t q)
{
    // Delete the ancilla only if whole line is empty
}

void QCirMgr::appendGate(string type, vector<size_t> bits)
{
    // QCirGate *temp = new QCirGate(_gateId, 0);
    QCirGate* temp=NULL;
    if(type=="h")   temp = new HGate(_gateId);
    else if(type=="z")   temp = new ZGate(_gateId);
    else if(type=="s")   temp = new SGate(_gateId);
    else if(type=="t")   temp = new TGate(_gateId);
    else if(type=="tdg")   temp = new TDGGate(_gateId);
    else if(type=="p")   temp = new PGate(_gateId,0);
    else if(type=="cz")   temp = new CZGate(_gateId);
    else if(type=="x")   temp = new XGate(_gateId);
    else if(type=="sx")   temp = new SXGate(_gateId);
    else if(type=="cx")   temp = new CXGate(_gateId);
    else if(type=="ccx")   temp = new CCXGate(_gateId);
    // Note: rz and p has a little difference
    else if(type=="rz")   temp = new CnRZGate(_gateId,0);
    else {
       cerr<<"The gate is not implement";
    }

    size_t max_time = 0;
    for (size_t k = 0; k < bits.size(); k++)
    {
        size_t q = bits[k];
        temp->addQubit(q, k == bits.size() - 1);
        if (_qubits[q]->getLast() != NULL)
        {
            temp->addParent(q, _qubits[q]->getLast());
            _qubits[q]->getLast()->addChild(q, temp);
            if ((_qubits[q]->getLast()->getTime() + 1) > max_time)
                max_time = _qubits[q]->getLast()->getTime() + 1;
        }
        else
            _qubits[q]->setFirst(temp);
        _qubits[q]->setLast(temp);
    }
    for (size_t k = 0; k < bits.size(); k++)
        temp->setTime(max_time);

    _qgate.push_back(temp);
    _gateId++;
}

void QCirMgr::removeGate(size_t id)
{

}

bool QCirMgr::parseQASM(string filename)
{
    // read file and open
    fstream qasm_file;
    string tmp;
    vector<string> record;
    qasm_file.open(filename.c_str(), ios::in);
    if (!qasm_file.is_open())
    {
        cerr << "Cannot open QASM \"" << filename << "\"!!" << endl;
        return false;
    }
    string str;
    for (int i = 0; i < 6; i++)
    {
        // OPENQASM 2.0;
        // include "qelib1.inc";
        // qreg q[int];
        qasm_file >> str;
    }
    // For netlist
    int nqubit = stoi(str.substr(str.find("[") + 1, str.size() - str.find("[") - 3));
    addAncilla(nqubit);
    vector<string> single_list{"x", "sx", "s", "rz", "i", "h", "t", "tdg"};

    while (qasm_file >> str)
    {
        string space_delimiter = " ";
        string type = str.substr(0, str.find(" ")); 
        string phase = (str.find("(") != string::npos) ? str.substr(str.find("(")+1, str.length()-str.find("(")-2): "";
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
                pin_id.push_back(q); // targ
                appendGate(type, pin_id);
            }
            else
            {
                if (type != "creg" && type != "qreg")
                {
                    cerr << "Unseen Gate " << type << endl;
                    return false;
                }
                else
                    qasm_file >> str;
            }
        }
        else
        {
            // Parse
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
            pin_id.push_back(q1); // ctrl
            pin_id.push_back(q2); // targ
            appendGate(type, pin_id);
        }
    }
    return true;
}