/****************************************************************************
  FileName     [ qcirMgr.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir manager functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <vector>
#include <string>
#include <string.h>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <map>
#include "qcirMgr.h"

using namespace std;
QCirMgr *qCirMgr = 0;

QCirGate *QCirMgr::getGate(size_t id) const
{
    for (size_t i = 0; i < _qgate.size(); i++)
    {
        if (_qgate[i]->getId() == id)
            return _qgate[i];
    }
    return NULL;
}
QCirQubit *QCirMgr::getQubit(size_t id) const
{
    for (size_t i = 0; i < _qubits.size(); i++)
    {
        if (_qubits[i]->getId() == id)
            return _qubits[i];
    }
    return NULL;
}
void QCirMgr::printSummary()
{
    if (_dirty)
        updateGateTime();
    cout << "Listed by gate ID" << endl;
    for (size_t i = 0; i < _qgate.size(); i++)
    {
        _qgate[i]->printGate();
    }
}
void QCirMgr::printQubits()
{
    if (_dirty)
        updateGateTime();

    for (size_t i = 0; i < _qubits.size(); i++)
        _qubits[i]->printBitLine();
}
bool QCirMgr::printGateInfo(size_t id, bool showTime)
{
    if (getGate(id) != NULL)
    {
        if (showTime && _dirty)
            updateGateTime();
        getGate(id)->printGateInfo(showTime);
        return true;
    }
    else
    {
        cerr << "ERROR: id " << id << " not found!!" << endl;
        return false;
    }
}
void QCirMgr::addQubit(size_t num)
{
    for (size_t i = 0; i < num; i++)
    {
        QCirQubit *temp = new QCirQubit(_qubitId);
        _qubits.push_back(temp);
        _qubitId++;
    }
}
void QCirMgr::DFS(QCirGate *currentGate)
{
    currentGate->setVisited(_globalDFScounter);

    vector<BitInfo> Info = currentGate->getQubits();
    for (size_t i = 0; i < Info.size(); i++)
    {
        if ((Info[i])._child != NULL)
        {
            if (!((Info[i])._child->isVisited(_globalDFScounter)))
            {
                DFS((Info[i])._child);
            }
        }
    }
    _topoOrder.push(currentGate);
}
void QCirMgr::updateTopoOrder()
{
    _globalDFScounter++;
    QCirGate *dummy = new HGate(-1);

    for (size_t i = 0; i < _qubits.size(); i++)
    {
        dummy->addDummyChild(_qubits[i]->getFirst());
    }
    DFS(dummy);
    _topoOrder.pop(); // pop dummy
    assert(_topoOrder.size() == _qgate.size());
}
void QCirMgr::updateGateTime()
{
    updateTopoOrder();
    while (!_topoOrder.empty())
    {
        QCirGate *currentGate = _topoOrder.top();
        vector<BitInfo> Info = currentGate->getQubits();
        size_t max_time = 0;
        for (size_t i = 0; i < Info.size(); i++)
        {
            if (Info[i]._parent == NULL)
                continue;
            if (Info[i]._parent->getTime() + 1 > max_time)
                max_time = Info[i]._parent->getTime() + 1;
        }
        currentGate->setTime(max_time);
        _topoOrder.pop();
    }
    _dirty = false;
}
bool QCirMgr::removeQubit(size_t id)
{
    // Delete the ancilla only if whole line is empty
    QCirQubit *target = getQubit(id);
    if (target == NULL)
    {
        cerr << "ERROR: id " << id << " not found!!" << endl;
        return false;
    }
    else
    {
        if (target->getLast() != NULL || target->getFirst() != NULL)
        {
            cerr << "ERROR: id " << id << " is not an empty qubit!!" << endl;
            return false;
        }
        else
        {
            _qubits.erase(remove(_qubits.begin(), _qubits.end(), target), _qubits.end());
            return true;
        }
    }
}

void QCirMgr::addGate(string type, vector<size_t> bits, Phase phase, bool append)
{
    QCirGate *temp = NULL;
    for_each(type.begin(), type.end(), [](char & c) {
        c = ::tolower(c);
    });
    if (type == "h")
        temp = new HGate(_gateId);
    else if (type == "z")
        temp = new ZGate(_gateId);
    else if (type == "s" )
        temp = new SGate(_gateId);
    else if (type == "sdg" || type == "s*" )
        temp = new SDGGate(_gateId);
    else if (type == "t" )
        temp = new TGate(_gateId);
    else if (type == "tdg" || type == "t*")
        temp = new TDGGate(_gateId);
    else if (type == "p")
        temp = new RZGate(_gateId);
    else if (type == "cz")
        temp = new CZGate(_gateId);
    else if (type == "x" || type == "X" || type == "not")
        temp = new XGate(_gateId);
    else if (type == "sx" || type == "x_1_2")
        temp = new SXGate(_gateId);
    else if (type == "cx" || type == "cnot")
        temp = new CXGate(_gateId);
    else if (type == "ccx")
        temp = new CCXGate(_gateId);
    // Note: rz and p has a little difference
    else if (type == "rz"){
        temp = new RZGate(_gateId);
        temp->setRotatePhase(phase);
    }   
       
    else
    {
        cerr << "Error: The gate "<< type << " is not implement!!"<<endl;
        return;
    }
    if(append){
        size_t max_time = 0;
        for (size_t k = 0; k < bits.size(); k++)
        {
            size_t q = bits[k];
            temp->addQubit(q, k == bits.size() - 1); // target is the last one
            QCirQubit *target = getQubit(q);
            if (target->getLast() != NULL)
            {
                temp->setParent(q, target->getLast());
                target->getLast()->setChild(q, temp);
                if ((target->getLast()->getTime() + 1) > max_time)
                    max_time = target->getLast()->getTime() + 1;
            }
            else
                target->setFirst(temp);
            target->setLast(temp);
        }
        temp->setTime(max_time);
    }
    else{
        for (size_t k = 0; k < bits.size(); k++)
        {
            size_t q = bits[k];
            temp->addQubit(q, k == bits.size() - 1); // target is the last one
            QCirQubit *target = getQubit(q);
            if(target->getFirst()!=NULL)
            {
                temp->setChild(q, target->getFirst());
                target->getFirst()->setParent(q, temp);
            }
            else
                target->setLast(temp);
            target->setFirst(temp);
        }
        _dirty = true;
    }
    _qgate.push_back(temp);
    _gateId++;
}

bool QCirMgr::removeGate(size_t id)
{
    QCirGate *target = getGate(id);
    if (target == NULL)
    {
        cerr << "ERROR: id " << id << " not found!!" << endl;
        return false;
    }
    else
    {
        vector<BitInfo> Info = target->getQubits();
        for (size_t i = 0; i < Info.size(); i++)
        {
            if (Info[i]._parent != NULL)
                Info[i]._parent->setChild(Info[i]._qubit, Info[i]._child);
            else
                getQubit(Info[i]._qubit)->setFirst(Info[i]._child);
            if (Info[i]._child != NULL)
                Info[i]._child->setParent(Info[i]._qubit, Info[i]._parent);
            else
                getQubit(Info[i]._qubit)->setLast(Info[i]._parent);
            Info[i]._parent = NULL;
            Info[i]._child = NULL;
        }
        _qgate.erase(remove(_qgate.begin(), _qgate.end(), target), _qgate.end());
        _dirty = true;
        return true;
    }
}

bool QCirMgr::parse(string filename)
{
    string extension = filename.substr(filename.find_last_of('.'), filename.size());
    if (extension == ".qasm") return parseQASM(filename);
    else if (extension == ".qc") return parseQC(filename);
    else if (extension == ".qsim") return parseQSIM(filename);
    else 
    {
        cerr << "Do not support the file extension " << extension << endl;
        return false;    
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
    // For netlist
    int nqubit = stoi(str.substr(str.find("[") + 1, str.size() - str.find("[") - 3));
    addQubit(nqubit);
    vector<string> single_list{"x", "sx", "s", "rz", "i", "h", "t", "tdg"};

    while (qasm_file >> str)
    {
        cerr << str << endl;
        string space_delimiter = " ";
        string type = str.substr(0, str.find(" "));
        string phaseStr = (str.find("(") != string::npos) ? str.substr(str.find("(") + 1, str.length() - str.find("(") - 2) : "0";
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
                Phase phase;
                phase.fromString(phaseStr);
                addGate(type, pin_id, phase,true);
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
            addGate(type, pin_id, Phase(0),true);
        }
    }
    return true;
}

bool QCirMgr::parseQC(string filename)
{
    // read file and open
    fstream qc_file;
    qc_file.open(filename.c_str(), ios::in);
    if (!qc_file.is_open())
    {
        cerr << "Cannot open QC file \"" << filename << "\"!!" << endl;
        return false;
    }

    // ex: qubit_labels = {A,B,C,1,2,3,result}
    //     qubit_id = distance(qubit_labels.begin(), find(qubit_labels.begin(), qubit_labels.end(), token));
    vector<string> qubit_labels;
    qubit_labels.clear();
    string line;
    vector<string> single_list{"X", "Z", "S", "S*", "H", "T", "T*"};
    vector<string> double_list{"cnot" , "cx", "cz"};
    size_t n_qubit=0;

    while ( getline(qc_file, line))
    {
        if (line.find('.')==0) // find initial statement
        {
            // erase .v .i or .o
            line.erase(0, line.find(' '));
            line.erase(0, 1);

            while (!line.empty())
            {
                string token= line.substr(0, line.find(' '));
                // Fix '\r'
                token = token[token.size()-1]=='\r' ? token.substr(0,token.size()-1) : token;
                if ( find(qubit_labels.begin(), qubit_labels.end(), token) == qubit_labels.end())
                {
                    qubit_labels.push_back(token);
                    n_qubit++;
                }
                line.erase(0, line.find(' '));
                line.erase(0, 1);
            }
        }
        else if (line.find('#')==0 || line == "" || line=="\r") continue;
        else if (line.find("BEGIN")==0)
        {
            addQubit(n_qubit);
        }
        else if (line.find("END")==0)
        {
            return true;
        }
        else // find a gate
        {
            string type = line.substr(0, line.find(' '));
            line.erase(0, line.find(' ')+1);
            //for (string label:qubit_labels) cerr << label << " " ;
            if ( find(single_list.begin(),single_list.end(),type)!=single_list.end())
            {
                //add single gate
                while (!line.empty())
                {
                    vector<size_t> pin_id;
                    bool singleTarget = (line.find(' ') == string::npos);
                    string qubit_label;
                    qubit_label =  singleTarget ? line.substr(0,line.find('\r')) : line.substr(0, line.find(' '));
                    size_t qubit_id = distance(qubit_labels.begin(), find(qubit_labels.begin(), qubit_labels.end(), qubit_label));
                    //phase phase;
                    pin_id.push_back(qubit_id);
                    addGate(type,pin_id,Phase(0),true);
                    line.erase(0, line.find(' '));
                    line.erase(0, 1);
                }
            }
            else if ( find(double_list.begin(),double_list.end(),type)!=double_list.end())
            {
                //add double gate
                vector<size_t> pin_id;

                while (!line.empty())
                {
                    bool singleTarget = (line.find(' ') == string::npos);
                    string qubit_label;
                    qubit_label =  singleTarget ? line.substr(0,line.find('\r')) : line.substr(0, line.find(' '));
                    int qubit_id = distance(qubit_labels.begin(), find(qubit_labels.begin(), qubit_labels.end(), qubit_label));
                    pin_id.push_back(qubit_id);
                    line.erase(0, line.find(' '));
                    line.erase(0, 1);

                }
                addGate(type,pin_id,Phase(0),true);
            }
            else if (type == "tof")
            {
                //add toffoli (not ,cnot or ccnot)
                vector<size_t> pin_id;

                while (!line.empty())
                {
                    
                    bool singleTarget = (line.find(' ') == string::npos);
                    string qubit_label;
                    qubit_label =  singleTarget ? line.substr(0,line.find('\r')) : line.substr(0, line.find(' '));
                    int qubit_id = distance(qubit_labels.begin(), find(qubit_labels.begin(), qubit_labels.end(), qubit_label));
                    pin_id.push_back(qubit_id);
                    line.erase(0, line.find(' '));
                    line.erase(0, 1);

                }

                if (pin_id.size()==1){addGate("X", pin_id, Phase(0),true);}
                else if (pin_id.size()==2){addGate("cnot", pin_id, Phase(0),true);}
                else if (pin_id.size()==3){addGate("ccx", pin_id, Phase(0),true);}
                else {cerr << "Do not support more than 2 control toffoli " << endl;}
            }
            else{ 
                cerr << "Find a undefined gate: "<< type << endl;
                return false;
            }
        }
    }
    return true;
}

bool QCirMgr::parseQSIM(string filename){
    // read file and open
    fstream qsim_file;
    qsim_file.open(filename.c_str(), ios::in);
    if (!qsim_file.is_open())
    {
        cerr << "Cannot open QSIM file \"" << filename << "\"!!" << endl;
        return false;
    }

    string n_qubitStr;
    string time, type;
    size_t qubit_control , qubit_target;
    string phaseStr;
    vector<string> single_list{"x", "y", "z", "h", "t", "x_1_2", "y_1_2", "rx", "rz", "s"};
    vector<string> double_list{"cx", "cz"};


    // decide qubit number
    int n_qubit;
    qsim_file >> n_qubit;
    //cerr << n_qubit << endl;
    addQubit(n_qubit);

    // add the gate
    
    while (qsim_file >> time >> type)
    {
        
        //cerr << time << type << endl;
        if ( find(single_list.begin(),single_list.end(),type)!=single_list.end())
        {
            // add single qubit gate
            vector<size_t> pin_id;
            qsim_file>> qubit_target;
            pin_id.push_back(qubit_target);
            //cerr << "Single gate on " << pin_id[0] << "?"<< endl;

            if (type == "rx" || type == "rz")
            {
                qsim_file >> phaseStr;
                Phase phase;
                phase.fromString(phaseStr);
                addGate(type , pin_id, phase, true);
                //cerr << "Add " << type << " on qubit "<< qubit_target << " with phase = " << phaseStr << endl;;

            }
            else
            {
                //cerr << "Add " << type << " on qubit "<< pin_id[0] << endl;
                addGate(type, pin_id, Phase(0), true);
                
            }

            continue;
        }
        else if (find(double_list.begin(),double_list.end(),type)!=double_list.end())
        {
            // add double qubit gate
            vector<size_t> pin_id;
            qsim_file>> qubit_control >>qubit_target;
            pin_id.push_back(qubit_control);
            pin_id.push_back(qubit_target);
            //cerr << "Double gate on " << qubit_control <<qubit_target << endl;
            addGate(type, pin_id, Phase(0), true);
            continue;
        }
        else
        {
            cerr << "Find a undefined gate: "<< type << endl;
            return false;
        }
    }
    return true;

    // qccr benchmark/qsim/circuits/circuit_q24.qsim
    // qccr benchmark/qsim/example.qsim
}