/****************************************************************************
  FileName     [ qcirReader.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Reader functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t
#include <fstream>  // for fstream
#include <iostream>
#include <string>  // for string

#include "qcir.h"  // for QCir

using namespace std;

/**
 * @brief Read QCir file
 *
 * @param filename
 * @return true if successfully read
 * @return false if error in file or not found
 */
bool QCir::readQCirFile(string filename) {
    string lastname = filename.substr(filename.find_last_of('/') + 1);

    string extension = (lastname.find('.') != string::npos) ? lastname.substr(lastname.find_last_of('.')) : "";

    if (extension == ".qasm")
        return readQASM(filename);
    else if (extension == ".qc")
        return readQC(filename);
    else if (extension == ".qsim")
        return readQSIM(filename);
    else if (extension == ".quipper")
        return readQUIPPER(filename);
    else if (extension == "") {
        fstream verify;
        verify.open(filename.c_str(), ios::in);
        if (!verify.is_open()) {
            cerr << "Cannot open the file \"" << filename << "\"!!" << endl;
            return false;
        }
        string first_item;
        verify >> first_item;

        if (first_item == "Inputs:")
            return readQUIPPER(filename);
        else if (isdigit(first_item[0]))
            return readQSIM(filename);
        else {
            cerr << "Do not support the file" << filename << endl;
            return false;
        }
        return true;
    } else {
        cerr << "Do not support the file extension " << extension << endl;
        return false;
    }
}

/**
 * @brief Read QASM
 *
 * @param filename
 * @return true if successfully read
 * @return false if error in file or not found
 */
bool QCir::readQASM(string filename) {
    // read file and open
    string lastname = filename.substr(filename.find_last_of('/') + 1);
    setFileName(lastname.substr(0, lastname.size() - 5));
    _procedures.clear();
    fstream qasm_file;
    string tmp;
    vector<string> record;
    qasm_file.open(filename.c_str(), ios::in);
    if (!qasm_file.is_open()) {
        cerr << "Cannot open QASM \"" << filename << "\"!!" << endl;
        return false;
    }
    string str;
    for (int i = 0; i < 6; i++) {
        // OPENQASM 2.0;
        // include "qelib1.inc";
        // qreg q[int];
        qasm_file >> str;
    }
    // For netlist
    string tok;

    size_t nqubit = stoul(str.substr(str.find("[") + 1, str.size() - str.find("[") - 3));
    addQubit(nqubit);
    getline(qasm_file, str);
    while (getline(qasm_file, str)) {
        string type;
        size_t type_end = myStrGetTok(str, type);
        string phaseStr = "0";
        if (myStrGetTok(str, phaseStr, 0, '(') != string::npos) {
            size_t stop = myStrGetTok(str, type, 0, '(');
            myStrGetTok(str, phaseStr, stop + 1, ')');
        } else
            phaseStr = "0";
        if (type == "creg" || type == "qreg" || type == "") {
            continue;
        }
        vector<size_t> pin_id;
        string tmp;
        string qub;
        size_t n = myStrGetTok(str, tmp, type_end, ',');
        while (tmp.size()) {
            myStrGetTok(tmp, qub, myStrGetTok(tmp, qub, 0, '[') + 1, ']');
            unsigned qub_n;
            if (!myStr2Uns(qub, qub_n) || qub_n >= nqubit) {
                cerr << "Error line: " << str << endl;
                return false;
            }
            pin_id.push_back(qub_n);
            n = myStrGetTok(str, tmp, n, ',');
        }

        Phase phase;
        Phase::fromString(phaseStr, phase);
        addGate(type, pin_id, phase, true);
    }
    updateGateTime();
    return true;
}

/**
 * @brief Read QC
 *
 * @param filename
 * @return true if successfully read
 * @return false if error in file or not found
 */
bool QCir::readQC(string filename) {
    // read file and open
    fstream qc_file;
    qc_file.open(filename.c_str(), ios::in);
    if (!qc_file.is_open()) {
        cerr << "Cannot open QC file \"" << filename << "\"!!" << endl;
        return false;
    }

    // ex: qubit_labels = {A,B,C,1,2,3,result}
    //     qubit_id = distance(qubit_labels.begin(), find(qubit_labels.begin(), qubit_labels.end(), token));
    vector<string> qubit_labels;
    qubit_labels.clear();
    string line;
    size_t n_qubit = 0;

    while (getline(qc_file, line)) {
        line = line[line.size() - 1] == '\r' ? line.substr(0, line.size() - 1) : line;

        if (line.find('.') == 0)  // find initial statement
        {
            // erase .v .i or .o
            line.erase(0, line.find(' ') + 1);
            size_t pos = 0;
            while (pos != string::npos) {
                string token;

                pos = myStrGetTok(line, token, pos);
                if (!count(qubit_labels.begin(), qubit_labels.end(), token)) {
                    qubit_labels.push_back(token);
                    n_qubit++;
                }
            }
        } else if (line.find('#') == 0 || line == "")
            continue;
        else if (line.find("BEGIN") == 0) {
            addQubit(n_qubit);
        } else if (line.find("END") == 0) {
            return true;
        } else  // find a gate
        {
            string type, qubit_label;
            vector<size_t> pin_id;
            size_t pos = 0;
            pos = myStrGetTok(line, type, pos);
            while (pos != string::npos) {
                pos = myStrGetTok(line, qubit_label, pos);
                if (count(qubit_labels.begin(), qubit_labels.end(), qubit_label)) {
                    size_t qubit_id = distance(qubit_labels.begin(), find(qubit_labels.begin(), qubit_labels.end(), qubit_label));
                    pin_id.push_back(qubit_id);
                } else {
                    cerr << "Error: Find a undefined qubit " << qubit_label << endl;
                    return false;
                }
            }
            if (type == "Tof" || type == "tof") {
                if (pin_id.size() == 1)
                    addGate("x", pin_id, Phase(0), true);
                else if (pin_id.size() == 2)
                    addGate("cx", pin_id, Phase(0), true);
                else if (pin_id.size() == 3)
                    addGate("ccx", pin_id, Phase(0), true);
                else {
                    cerr << "Error: Do not support Tof gate with more than 2 controls" << endl;
                    return false;
                }
            } else
                addGate(type, pin_id, Phase(0), true);
        }
    }
    return true;
}

/**
 * @brief Read QSIM
 *
 * @param filename
 * @return true if successfully read
 * @return false if error in file or not found
 */
bool QCir::readQSIM(string filename) {
    // read file and open
    fstream qsim_file;
    qsim_file.open(filename.c_str(), ios::in);
    if (!qsim_file.is_open()) {
        cerr << "Cannot open QSIM file \"" << filename << "\"!!" << endl;
        return false;
    }

    string n_qubitStr, line;
    vector<string> single_gate_list{"x", "y", "z", "h", "t", "x_1_2", "y_1_2", "rx", "rz", "s"};
    // decide qubit number
    int n_qubit;
    getline(qsim_file, line);
    n_qubit = stoi(line);
    addQubit(n_qubit);

    // add the gate
    // Todo: implentment hz_1_2 gate and fs gate

    while (getline(qsim_file, line)) {
        if (line == "") continue;
        string time, type, phaseStr, qubit_id;
        vector<size_t> pin_id;
        size_t pos = 0;
        pos = myStrGetTok(line, time, pos);
        pos = myStrGetTok(line, type, pos);
        if (type == "cx" || type == "cz") {
            // add 2 qubit gate
            pos = myStrGetTok(line, qubit_id, pos);
            pin_id.push_back(stoul(qubit_id));
            pos = myStrGetTok(line, qubit_id, pos);
            pin_id.push_back(stoul(qubit_id));
            addGate(type, pin_id, Phase(0), true);
        } else if (type == "rx" || type == "rz") {
            // add phase gate
            Phase phase;
            pos = myStrGetTok(line, qubit_id, pos);
            pin_id.push_back(stoul(qubit_id));
            pos = myStrGetTok(line, phaseStr, pos);
            Phase::fromString(phaseStr, phase);
            addGate(type, pin_id, phase, true);
        } else if (count(single_gate_list.begin(), single_gate_list.end(), type)) {
            // add single qubit gate
            pos = myStrGetTok(line, qubit_id, pos);
            pin_id.push_back(stoul(qubit_id));
            addGate(type, pin_id, Phase(0), true);
        } else {
            cerr << "Error: Do not support gate type " << type << endl;
            return false;
        }
    }
    return true;
}

/**
 * @brief Read QUIPPER
 *
 * @param filename
 * @return true if successfully read
 * @return false if error in file or not found
 */
bool QCir::readQUIPPER(string filename) {
    // read file and open
    fstream quipper_file;
    quipper_file.open(filename.c_str(), ios::in);
    if (!quipper_file.is_open()) {
        cerr << "Cannot open QUIPPER file \"" << filename << "\"!!" << endl;
        return false;
    }

    string line;
    size_t n_qubit;
    vector<string> single_list{"X", "T", "S", "H", "Z", "not"};

    // Count qubit number
    getline(quipper_file, line);
    n_qubit = count(line.begin(), line.end(), 'Q');
    addQubit(n_qubit);

    while (getline(quipper_file, line)) {
        if (line.find("QGate") == 0) {
            // addgate
            string type = line.substr(line.find("[") + 2, line.find("]") - line.find("[") - 3);
            size_t qubit_target;
            if (find(single_list.begin(), single_list.end(), type) != single_list.end()) {
                qubit_target = stoul(line.substr(line.find("(") + 1, line.find(")") - line.find("(") - 1));
                vector<size_t> pin_id;

                if (line.find("controls=") != string::npos) {
                    // have control
                    string ctrls_info;
                    ctrls_info = line.substr(line.find_last_of("[") + 1, line.find_last_of("]") - line.find_last_of("[") - 1);

                    if (ctrls_info.find(to_string(qubit_target)) != string::npos) {
                        cerr << "Error: Control qubit and target are the same." << endl;
                        return false;
                    }

                    if (count(line.begin(), line.end(), '+') == 1) {
                        // one control
                        if (type != "not" && type != "X" && type != "Z") {
                            cerr << "Error: Control gate only support on \'cnot\', \'CX\' and \'CZ\'" << endl;
                            return false;
                        }
                        size_t qubit_control = stoul(ctrls_info.substr(1));
                        pin_id.push_back(qubit_control);
                        pin_id.push_back(qubit_target);
                        type.insert(0, "C");
                        addGate(type, pin_id, Phase(0), true);
                    } else if (count(line.begin(), line.end(), '+') == 2) {
                        // 2 controls
                        if (type != "not" && type != "X" && type != "Z") {
                            cerr << "Error: Toffoli gate only support \'ccx\'and \'ccz\'" << endl;
                            return false;
                        }
                        size_t qubit_control1, qubit_control2;
                        qubit_control1 = stoul(ctrls_info.substr(1, ctrls_info.find(',') - 1));
                        qubit_control2 = stoul(ctrls_info.substr(ctrls_info.find(',') + 2));
                        pin_id.push_back(qubit_control1);
                        pin_id.push_back(qubit_control2);
                        pin_id.push_back(qubit_target);
                        type.insert(0, "CC");
                        addGate(type, pin_id, Phase(0), true);
                    } else {
                        cerr << "Error: Unsupport more than 2 controls gate." << endl;
                        return false;
                    }
                } else {
                    // without control
                    pin_id.push_back(qubit_target);
                    addGate(type, pin_id, Phase(0), true);
                }

            } else {
                cerr << "Find a undefined gate: " << type << endl;
                return false;
            }
            continue;
        } else if (line.find("Outputs") == 0) {
            return true;
        } else if (line.find("Comment") == 0 || line.find("QTerm0") == 0 || line.find("QMeas") == 0 || line.find("QDiscard") == 0)
            continue;
        else if (line.find("QInit0") == 0) {
            cerr << "Unsupported expression: QInit0" << endl;
            return false;
        } else if (line.find("QRot") == 0) {
            cerr << "Unsupported expression: QRot" << endl;
            return false;
        } else {
            cerr << "Unsupported expression: " << line << endl;
        }
    }
    return true;
}
