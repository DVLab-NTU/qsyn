/****************************************************************************
  FileName     [ qcir.cpp ]
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
#include <cassert>
#include "zxGraphMgr.h"
#include "qcir.h"

using namespace std;
QCir *qCir = 0;
extern ZXGraphMgr *zxGraphMgr;
extern size_t verbose;

QCirGate *QCir::getGate(size_t id) const
{
    for (size_t i = 0; i < _qgate.size(); i++)
    {
        if (_qgate[i]->getId() == id)
            return _qgate[i];
    }
    return NULL;
}
QCirQubit *QCir::getQubit(size_t id) const
{
    for (size_t i = 0; i < _qubits.size(); i++)
    {
        if (_qubits[i]->getId() == id)
            return _qubits[i];
    }
    return NULL;
}
void QCir::printSummary()
{
    if (_dirty)
        updateGateTime();
    cout << "Listed by gate ID" << endl;
    for (size_t i = 0; i < _qgate.size(); i++)
    {
        _qgate[i]->printGate();
    }
}
void QCir::printQubits()
{
    if (_dirty)
        updateGateTime();

    for (size_t i = 0; i < _qubits.size(); i++)
        _qubits[i]->printBitLine();
}
bool QCir::printGateInfo(size_t id, bool showTime)
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
        cerr << "Error: id " << id << " not found!!" << endl;
        return false;
    }
}
void QCir::addQubit(size_t num)
{
    for (size_t i = 0; i < num; i++)
    {
        QCirQubit *temp = new QCirQubit(_qubitId);
        _qubits.push_back(temp);
        _qubitId++;
        clearMapping(); 
    }
}
void QCir::DFS(QCirGate *currentGate)
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
    _topoOrder.push_back(currentGate);
}
void QCir::updateTopoOrder()
{
    _topoOrder.clear();
    _globalDFScounter++;
    QCirGate *dummy = new HGate(-1);

    for (size_t i = 0; i < _qubits.size(); i++)
    {
        dummy->addDummyChild(_qubits[i]->getFirst());
    }
    DFS(dummy);
    _topoOrder.pop_back(); // pop dummy
    reverse(_topoOrder.begin(), _topoOrder.end());
    assert(_topoOrder.size() == _qgate.size());
}
// An easy checker for lambda function
bool QCir::printTopoOrder()
{
    auto testLambda = [](QCirGate *G){ cout << G->getId() << endl; };
    topoTraverse(testLambda);
    return true;
}
void QCir::updateGateTime()
{
    auto Lambda = [](QCirGate *currentGate)
    {
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
    };
    topoTraverse(Lambda);
}
void QCir::printZXTopoOrder()
{
    auto Lambda = [this](QCirGate *G)
    {
        cout << "Gate " << G->getId() << " (" << G->getTypeStr() << ")" << endl;
        ZXGraph* tmp = G->getZXform(_ZXNodeId);
        tmp -> printVertices();
    };
    topoTraverse(Lambda);
}
void QCir::clearMapping()
{
    for(size_t i=0; i<_ZXGraphList.size(); i++){
        cerr << "Note: Graph "<< _ZXGraphList[i]->getId() << " is deleted due to modification(s) !!" << endl;
        _ZXGraphList[i] -> reset();
        zxGraphMgr -> removeZXGraph(_ZXGraphList[i] -> getId());
    }
    _ZXGraphList.clear();
}
void QCir::mapping()
{
    if(zxGraphMgr == 0){
        cerr << "Error: ZXMODE is OFF, please turn on before mapping" << endl;
        return;
    }
    updateTopoOrder();
    // _ZXG->clearPtrs(); Cannot clear ptr since storing in zxGraphMgr
    // _ZXG->reset();
    // delete _ZXG; Cannot clear ptr since storing in zxGraphMgr
    ZXGraph* _ZXG = zxGraphMgr -> addZXGraph(zxGraphMgr->getNextID());
    _ZXG -> setRef((void**)_ZXG);
    
    
    _ZXNodeId = 0;
    size_t maxInput = 0;
    if(verbose >= 3) cout << "----------- ADD BOUNDARIES -----------" << endl;
    for(size_t i=0; i<_qubits.size(); i++){
        if (_qubits[i]->getId() > maxInput)
            maxInput = _qubits[i]->getId();
        _ZXG -> setInputHash(_qubits[i]->getId(), _ZXG -> addInput( 2*(_qubits[i]->getId()), _qubits[i]->getId()));
        _ZXG -> setOutputHash(_qubits[i]->getId(), _ZXG -> addOutput( 2*(_qubits[i]->getId()) + 1, _qubits[i]->getId()));
        _ZXG -> addEdgeById( 2*(_qubits[i]->getId()), 2*(_qubits[i]->getId()) + 1, EdgeType::SIMPLE);
        if(verbose >= 3)  cout << "Add Qubit " << _qubits[i]->getId() << " inp: " << 2*(_qubits[i]->getId()) << " oup: " << 2*(_qubits[i]->getId())+1 << endl;
    }
    _ZXNodeId = 2*(maxInput+ 1)-1;
    if(verbose >= 5) cout << "ZXnode starts from " << _ZXNodeId << endl;
    if(verbose >= 3) cout << "--------------------------------------" << endl << endl;

    auto Lambda = [this, _ZXG](QCirGate *G)
    {
        if(verbose >= 3) cout << "Gate " << G->getId() << " (" << G->getTypeStr() << ")" << endl;
        ZXGraph* tmp = G->getZXform(_ZXNodeId);
        if(tmp == NULL){
            cerr << "Mapping of gate "<< G->getId()<< " (type: " << G->getTypeStr() << ") not implemented, the mapping result is wrong!!" <<endl;
            return;
        }
        if(verbose >= 5) cout << "********** CONCATENATION **********" << endl;
        _ZXG -> concatenate(tmp, false);
        if(verbose >= 5) cout << "***********************************" << endl;
        if(verbose >= 3)  cout << "--------------------------------------" << endl;
    };

    if(verbose >= 3)  cout << "---- TRAVERSE AND BUILD THE GRAPH ----" << endl;
    topoTraverse(Lambda);
    _ZXG -> cleanRedundantEdges();
    if(verbose >= 3)  cout << "--------------------------------------" << endl;
    if(verbose >= 3)  cout << "---------------------------------- GRAPH INFORMATION ---------------------------------" << endl;
    _ZXG -> printVertices();
    if(verbose >= 3)  cout << "--------------------------------------------------------------------------------------" << endl; 
    if(verbose >= 7) {
        zxGraphMgr -> printZXGraphMgr();
    }
    _ZXGraphList.push_back(_ZXG);
}
bool QCir::removeQubit(size_t id)
{
    // Delete the ancilla only if whole line is empty
    QCirQubit *target = getQubit(id);
    if (target == NULL)
    {
        cerr << "Error: id " << id << " not found!!" << endl;
        return false;
    }
    else
    {
        if (target->getLast() != NULL || target->getFirst() != NULL)
        {
            cerr << "Error: id " << id << " is not an empty qubit!!" << endl;
            return false;
        }
        else
        {
            _qubits.erase(remove(_qubits.begin(), _qubits.end(), target), _qubits.end());
            clearMapping();   
            return true;
        }
    }
}

void QCir::addGate(string type, vector<size_t> bits, Phase phase, bool append)
{
    QCirGate *temp = NULL;
    for_each(type.begin(), type.end(), [](char &c)
             { c = ::tolower(c); });
    if (type == "h")
        temp = new HGate(_gateId);
    else if (type == "z")
        temp = new ZGate(_gateId);
    else if (type == "s")
        temp = new SGate(_gateId);
    else if (type == "t")
        temp = new TGate(_gateId);
    else if (type == "tdg")
        temp = new TDGGate(_gateId);
    else if (type == "p")
        temp = new RZGate(_gateId);
    else if (type == "cz")
        temp = new CZGate(_gateId);
    else if (type == "x" || type == "not")
        temp = new XGate(_gateId);
    else if (type == "y")
        temp = new YGate(_gateId);
    else if (type == "sx" || type == "x_1_2")
        temp = new SXGate(_gateId);
    else if (type == "sy" || type == "y_1_2")
        temp = new SYGate(_gateId);
    else if (type == "cx" || type == "cnot")
        temp = new CXGate(_gateId);
    else if (type == "ccx" || type== "ccnot")
        temp = new CCXGate(_gateId);
    else if (type == "ccz")
        temp = new CCZGate(_gateId);
    // Note: rz and p has a little difference
    else if (type == "rz")
    {
        temp = new RZGate(_gateId);
        temp->setRotatePhase(phase);
    }  
    else if (type == "rx"){
        temp = new RXGate(_gateId);
        temp->setRotatePhase(phase);
    } 
    else
    {
        cerr << "Error: The gate " << type << " is not implement!!" << endl;
        return;
    }
    if (append)
    {
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
    else
    {
        for (size_t k = 0; k < bits.size(); k++)
        {
            size_t q = bits[k];
            temp->addQubit(q, k == bits.size() - 1); // target is the last one
            QCirQubit *target = getQubit(q);
            if (target->getFirst() != NULL)
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
    clearMapping(); 
}

bool QCir::removeGate(size_t id)
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
        clearMapping(); 
        return true;
    }
}

bool QCir::parse(string filename)
{
    string lastname = filename.substr(filename.find_last_of('/')+1);
    //cerr << lastname << endl;
    string extension = (lastname.find('.')!= string::npos) ? lastname.substr(lastname.find_last_of('.')):"";
    //cerr << extension << endl;
    if (extension == ".qasm") return parseQASM(filename);
    else if (extension == ".qc") return parseQC(filename);
    else if (extension == ".qsim") return parseQSIM(filename);
    else if (extension == ".quipper") return parseQUIPPER(filename);
    else if (extension == ""){
        fstream verify;
        verify.open(filename.c_str(), ios::in);
        if (!verify.is_open()){
            cerr << "Cannot open the file \"" << filename << "\"!!" << endl;
            return false;
        }
        string first_item;
        verify >> first_item;
        //cerr << first_item << endl;

        if (first_item == "Inputs:") return parseQUIPPER(filename);
        else if (isdigit(first_item[0])) return parseQSIM(filename);
        else{
            cerr << "Do not support the file" << filename << endl;
            return false;
        }
        return true;
    }
    else 
    {
        cerr << "Do not support the file extension " << extension << endl;
        return false;    
    }
}

bool QCir::parseQASM(string filename)
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
        //cerr << str << endl;
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
                addGate(type, pin_id, phase, true);
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
            addGate(type, pin_id, Phase(0), true);
        }
    }
    return true;
}

bool QCir::parseQC(string filename)
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

bool QCir::parseQSIM(string filename){
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
    size_t qubit_control, qubit_target;
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

bool QCir::parseQUIPPER(string filename){
    // read file and open
    fstream quipper_file;
    quipper_file.open(filename.c_str(), ios::in);
    if (!quipper_file.is_open())
    {
        cerr << "Cannot open QUIPPER file \"" << filename << "\"!!" << endl;
        return false;
    }

    string line;
    size_t n_qubit;
    vector<string> single_list{"X", "T", "S", "H", "Z","not"};

    // Count qubit number
    getline(quipper_file, line);
    n_qubit = count(line.begin(), line.end(), 'Q');
    addQubit(n_qubit);

    while (getline(quipper_file, line)){

        if (line.find("QGate")==0){
            // addgate
            string type= line.substr(line.find("[")+2,line.find("]")-line.find("[")-3);
            size_t qubit_target;
            if (find(single_list.begin(),single_list.end(),type)!=single_list.end()){
                qubit_target = stoul(line.substr(line.find("(")+1,line.find(")")-line.find("(")-1));
                vector<size_t> pin_id;
                //cerr << qubit_target << endl;

                if (line.find("controls=")!= string::npos){
                    // have control
                    string ctrls_info;
                    ctrls_info = line.substr(line.find_last_of("[")+1, line.find_last_of("]")-line.find_last_of("[")-1);
                    //cerr << ctrls_info << endl;

                    if (ctrls_info.find(to_string(qubit_target))!= string::npos){
                        cerr << "Error: Control qubit and target are the same." << endl;
                        return false;
                    }

                    if (count(line.begin(), line.end(), '+')==1){
                        // one control
                        if (type != "not" && type != "X" && type != "Z"){
                            cerr << "Error: Control gate only support on \'cnot\', \'CX\' and \'CZ\'" << endl;
                            return false;
                        }    
                        size_t qubit_control = stoul(ctrls_info.substr(1));
                        pin_id.push_back(qubit_control);
                        pin_id.push_back(qubit_target);
                        type.insert(0, "C");
                        addGate(type, pin_id, Phase(0), true);
                    }
                    else if (count(line.begin(), line.end(), '+')==2){
                        // 2 controls
                        if (type != "not" && type != "X" && type != "Z"){
                            cerr << "Error: Toffoli gate only support \'ccx\'and \'ccz\'" << endl;
                            return false;
                        }
                        size_t qubit_control1, qubit_control2;
                        qubit_control1 = stoul(ctrls_info.substr(1,ctrls_info.find(',')-1));
                        qubit_control2 = stoul(ctrls_info.substr(ctrls_info.find(',')+2));
                        //cerr << qubit_control1 << endl;
                        //cerr << qubit_control2 << endl;
                        pin_id.push_back(qubit_control1);
                        pin_id.push_back(qubit_control2);
                        pin_id.push_back(qubit_target);
                        type.insert(0, "CC");
                        addGate(type, pin_id, Phase(0), true);
                    }
                    else {
                        cerr << "Error: Unsupport more than 2 controls gate."<< endl;
                        return false;
                    }
                }
                else{
                    // without control
                    pin_id.push_back(qubit_target);
                    addGate(type, pin_id, Phase(0), true);
                }

            }
            else{
                cerr << "Find a undefined gate: "<< type << endl;
                return false;
            }
            continue;
        }
        else if (line.find("Outputs")==0){
            //cerr << "Done" << endl;
            return true;
        }
        else if (line.find("Comment")==0 || line.find("QTerm0")==0 || line.find("QMeas")==0 || line.find("QDiscard")==0 )
            continue;
        else if (line.find("QInit0")==0){
            cerr << "Unsupported expression: QInit0" << endl;
            return false;
        }
        else if (line.find("QRot")==0){
            cerr << "Unsupported expression: QRot" << endl;
            return false;
        }
        else{
            cerr << "Unsupported expression: " << line << endl;

        }

    }
    //qccr benchmark/quipper/example.quipper
    // qccr benchmark/quipper/adder_8_before.quipper
    return true;
}
