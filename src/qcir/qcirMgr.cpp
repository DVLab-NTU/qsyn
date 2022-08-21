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
    for (size_t i = 0; i < _bitFirst.size(); i++)
    {
        QCirGate *current = _bitFirst[i];
        size_t last_time = 0;
        cout << "Q" << setfill('0') << setw(2) << i << "  ";

        while (current != NULL)
        {
            cout << "-";
            while (last_time < current->getTime())
            {
                cout << "----";
                last_time++;
            }
            cout << setfill(' ') << setw(2) << current->getTypeStr().substr(0, 2);
            last_time = current->getTime() + 1;
            current = current->getQubit(i)._child;
            cout << "-";
        }
        cout << endl;
    }
}
void QCirMgr::initialLastExec()
{
    for (size_t i = 0; i < _nqubit; i++)
    {
        pair<size_t, size_t> init(-1, 0);
        _lastExec.push_back(init);
        _bitFirst.push_back(NULL);
    }
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
    size_t gate_id = 0;
    // For netlist
    _nqubit = stoi(str.substr(str.find("[") + 1, str.size() - str.find("[") - 3));
    initialLastExec();
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

                // construct data structure
                QCirGate *temp = new QCirGate(gate_id, type, 1);
                temp->addQubit(q, true);
                if (_lastExec[q].first != -1)
                {
                    temp->addParent(q, _qgate[_lastExec[q].first]);
                    _qgate[_lastExec[q].first]->addChild(q, temp);
                }
                else
                {
                    _bitFirst[q] = temp;
                }
                temp->setTime(_lastExec[q].second);

                // update and store
                _lastExec[q].first = gate_id;
                _lastExec[q].second++;
                _qgate.push_back(temp);
                gate_id++;
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
            QCirGate *temp = new QCirGate(gate_id, type, 2);
            // Data Structure
            size_t max_time = 0;
            for (size_t k = 0; k < pin_id.size(); k++)
            {
                size_t q = pin_id[k];
                temp->addQubit(q, k == pin_id.size() - 1);
                if (_lastExec[q].first != -1)
                {
                    temp->addParent(q, _qgate[_lastExec[q].first]);
                    _qgate[_lastExec[q].first]->addChild(q, temp);
                }
                else
                    _bitFirst[q] = temp;
                if (_lastExec[q].second > max_time)
                    max_time = _lastExec[q].second;
                // update and store
                _lastExec[q].first = gate_id;
            }
            for (size_t k = 0; k < pin_id.size(); k++)
            {
                _lastExec[pin_id[k]].second = max_time + 1;
                temp->setTime(max_time);
            }

            _qgate.push_back(temp);
            gate_id++;
        }
    }
    return true;
}