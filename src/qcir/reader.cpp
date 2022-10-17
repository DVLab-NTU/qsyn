/****************************************************************************
  FileName     [ reader.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir reader functions ]
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
#include "qcir.h"

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
    string tok;
    
    int nqubit = stoi(str.substr(str.find("[") + 1, str.size() - str.find("[") - 3));
    addQubit(nqubit);
    getline(qasm_file, str);
    while (getline(qasm_file, str))
    {
        string type;
        size_t type_end = myStrGetTok(str, type);
        string phaseStr = "0";
        if(myStrGetTok(str, phaseStr, 0, '(') != string::npos){
            size_t stop = myStrGetTok(str, type, 0, '(');
            myStrGetTok(str, phaseStr, stop+1, ')');
        }
        else 
            phaseStr = "0"; 
        if(type=="creg" || type=="qreg" || type==""){
            continue;
        }
        vector<size_t> pin_id;
        string tmp;
        string qub;
        size_t n = myStrGetTok(str, tmp, type_end, ',');
        while (tmp.size()) {

            myStrGetTok(tmp, qub, myStrGetTok(tmp, qub, 0, '[')+1, ']');
            unsigned qub_n;
            if(!myStr2Uns(qub, qub_n)) cerr << "Error line: " << str << endl;
            pin_id.push_back(qub_n);
            n = myStrGetTok(str, tmp, n, ',');
        }
        
        Phase phase;
        phase.fromString(phaseStr);
        addGate(type, pin_id, phase, true);
        
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
    addQubit(n_qubit);

    // add the gate
    
    while (qsim_file >> time >> type)
    {
        if ( find(single_list.begin(),single_list.end(),type)!=single_list.end())
        {
            // add single qubit gate
            vector<size_t> pin_id;
            qsim_file>> qubit_target;
            pin_id.push_back(qubit_target);

            if (type == "rx" || type == "rz")
            {
                qsim_file >> phaseStr;
                Phase phase;
                phase.fromString(phaseStr);
                addGate(type , pin_id, phase, true);
            }
            else
            {
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

                if (line.find("controls=")!= string::npos){
                    // have control
                    string ctrls_info;
                    ctrls_info = line.substr(line.find_last_of("[")+1, line.find_last_of("]")-line.find_last_of("[")-1);

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
    return true;
}
