/****************************************************************************
  FileName     [ qcirReader.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Reader functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <fstream>
#include <string>

#include "qcir/qcir.hpp"
#include "util/logger.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

extern dvlab::utils::Logger logger;

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
            logger.error("Cannot open the file \"{}\"!!", filename);
            return false;
        }
        string first_item;
        verify >> first_item;

        if (first_item == "Inputs:")
            return readQUIPPER(filename);
        else if (isdigit(first_item[0]))
            return readQSIM(filename);
        else {
            logger.error("Cannot derive the type of file \"{}\"!!", filename);
            return false;
        }
        return true;
    } else {
        logger.error("File format \"{}\" is not supported!!", extension);
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
    _procedures.clear();
    fstream qasm_file;
    string tmp;
    vector<string> record;
    qasm_file.open(filename.c_str(), ios::in);
    if (!qasm_file.is_open()) {
        logger.error("Cannot open the QASM file \"{}\"!!", filename);
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
                logger.error("invalid qubit id on line {}!!", str);
                return false;
            }
            pin_id.emplace_back(qub_n);
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
        logger.error("Cannot open the QC file \"{}\"!!", filename);
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
                    qubit_labels.emplace_back(token);
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
                    pin_id.emplace_back(qubit_id);
                } else {
                    logger.error("encountered a undefined qubit ({})!!", qubit_label);
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
                    logger.error("Toffoli gates with more than 2 controls are not supported!!");
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
        logger.error("Cannot open the QSIM file \"{}\"!!", filename);
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
            pin_id.emplace_back(stoul(qubit_id));
            myStrGetTok(line, qubit_id, pos);
            pin_id.emplace_back(stoul(qubit_id));
            addGate(type, pin_id, Phase(0), true);
        } else if (type == "rx" || type == "rz") {
            // add phase gate
            Phase phase;
            pos = myStrGetTok(line, qubit_id, pos);
            pin_id.emplace_back(stoul(qubit_id));
            myStrGetTok(line, phaseStr, pos);
            Phase::fromString(phaseStr, phase);
            addGate(type, pin_id, phase, true);
        } else if (count(single_gate_list.begin(), single_gate_list.end(), type)) {
            // add single qubit gate
            myStrGetTok(line, qubit_id, pos);
            pin_id.emplace_back(stoul(qubit_id));
            addGate(type, pin_id, Phase(0), true);
        } else {
            logger.error("Gate type {} is not supported!!", type);
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
        logger.error("Cannot open the QUIPPER file \"{}\"!!", filename);
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
                        logger.error("Control qubit and target cannot be the same!!");
                        return false;
                    }

                    if (count(line.begin(), line.end(), '+') == 1) {
                        // one control
                        if (type != "not" && type != "X" && type != "Z") {
                            logger.error("Unsupported controlled gate type!! Only `cnot`, `CX` and `CZ` are supported.");
                            return false;
                        }
                        size_t qubit_control = stoul(ctrls_info.substr(1));
                        pin_id.emplace_back(qubit_control);
                        pin_id.emplace_back(qubit_target);
                        type.insert(0, "C");
                        addGate(type, pin_id, Phase(0), true);
                    } else if (count(line.begin(), line.end(), '+') == 2) {
                        // 2 controls
                        if (type != "not" && type != "X" && type != "Z") {
                            logger.error("Unsupported doubly-controlled gate type!! Only `ccx` and `ccz` are supported.");
                            return false;
                        }
                        size_t qubit_control1, qubit_control2;
                        qubit_control1 = stoul(ctrls_info.substr(1, ctrls_info.find(',') - 1));
                        qubit_control2 = stoul(ctrls_info.substr(ctrls_info.find(',') + 2));
                        pin_id.emplace_back(qubit_control1);
                        pin_id.emplace_back(qubit_control2);
                        pin_id.emplace_back(qubit_target);
                        type.insert(0, "CC");
                        addGate(type, pin_id, Phase(0), true);
                    } else {
                        logger.error("Controlled gates with more than 2 controls are not supported!!");
                        return false;
                    }
                } else {
                    // without control
                    pin_id.emplace_back(qubit_target);
                    addGate(type, pin_id, Phase(0), true);
                }

            } else {
                logger.error("Unsupported gate type {}!!", type);
                return false;
            }
            continue;
        } else if (line.find("Outputs") == 0) {
            return true;
        } else if (line.find("Comment") == 0 || line.find("QTerm0") == 0 || line.find("QMeas") == 0 || line.find("QDiscard") == 0)
            continue;
        else if (line.find("QInit0") == 0) {
            logger.error("Unsupported expression: QInit0");
            return false;
        } else if (line.find("QRot") == 0) {
            logger.error("Unsupported expression: QRot");
            return false;
        } else {
            logger.error("Unsupported expression: {}", line);
        }
    }
    return true;
}
