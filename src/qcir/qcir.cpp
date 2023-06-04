/****************************************************************************
  FileName     [ qcir.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Edition functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir.h"

#include <stdlib.h>  // for abort

#include <cassert>  // for assert
#include <string>   // for string

#include "qcirGate.h"    // for QCirGate
#include "qcirQubit.h"   // for QCirQubit
#include "textFormat.h"  // for TextFormat

using namespace std;
namespace TF = TextFormat;
extern size_t verbose;

/**
 * @brief Get Gate.
 *
 * @param id
 * @return QCirGate*
 */
QCirGate *QCir::getGate(size_t id) const {
    for (size_t i = 0; i < _qgates.size(); i++) {
        if (_qgates[i]->getId() == id)
            return _qgates[i];
    }
    return NULL;
}

/**
 * @brief Get Qubit.
 *
 * @param id
 * @return QCirQubit
 */
QCirQubit *QCir::getQubit(size_t id) const {
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]->getId() == id)
            return _qubits[i];
    }
    return NULL;
}

/**
 * @brief Add procedures to QCir
 *
 * @param p
 * @param procedures
 */
void QCir::addProcedure(std::string p, const std::vector<std::string> &procedures) {
    for (auto pr : procedures)
        _procedures.emplace_back(pr);
    if (p != "") _procedures.emplace_back(p);
}

/**
 * @brief Print QCir Gates
 */
void QCir::printGates() {
    if (_dirty)
        updateGateTime();
    cout << "Listed by gate ID" << endl;
    for (size_t i = 0; i < _qgates.size(); i++) {
        _qgates[i]->printGate();
    }
}

/**
 * @brief Print Depth of QCir
 *
 */
void QCir::printDepth() {
    if (_dirty) updateGateTime();
    size_t depth = 0;
    for (size_t i = 0; i < _qgates.size(); i++) {
        if (_qgates[i]->getTime() > depth) depth = _qgates[i]->getTime();
    }
    cout << "Depth       : " << depth << endl;
}

/**
 * @brief Print QCir
 */
void QCir::printCircuit() {
    cout << "QCir " << _id << "( "
         << _qubits.size() << " qubits, "
         << _qgates.size() << " gates)\n";
}

/**
 * @brief Print QCir Summary
 */
void QCir::printSummary() {
    printCircuit();
    analysis();
    printDepth();
}

/**
 * @brief Print Qubits
 */
void QCir::printQubits() {
    if (_dirty)
        updateGateTime();

    for (size_t i = 0; i < _qubits.size(); i++)
        _qubits[i]->printBitLine();
}

/**
 * @brief Print Gate information
 *
 * @param id
 * @param showTime if true, show the time
 */
bool QCir::printGateInfo(size_t id, bool showTime) {
    if (getGate(id) != NULL) {
        if (showTime && _dirty)
            updateGateTime();
        getGate(id)->printGateInfo(showTime);
        return true;
    } else {
        cerr << "Error: id " << id << " not found!!" << endl;
        return false;
    }
}

/**
 * @brief Add single Qubit.
 *
 * @return QCirQubit*
 */
QCirQubit *QCir::addSingleQubit() {
    QCirQubit *temp = new QCirQubit(_qubitId);
    _qubits.push_back(temp);
    _qubitId++;
    clearMapping();
    return temp;
}

/**
 * @brief Insert single Qubit.
 *
 * @param id
 * @return QCirQubit*
 */
QCirQubit *QCir::insertSingleQubit(size_t id) {
    assert(getQubit(id) == NULL);
    QCirQubit *temp = new QCirQubit(id);
    size_t cnt = 0;
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]->getId() < id)
            cnt++;
        else
            break;
    }
    _qubits.insert(_qubits.begin() + cnt, temp);
    clearMapping();
    return temp;
}

/**
 * @brief Add Qubit.
 *
 * @param num
 */
void QCir::addQubit(size_t num) {
    for (size_t i = 0; i < num; i++) {
        QCirQubit *temp = new QCirQubit(_qubitId);
        _qubits.push_back(temp);
        _qubitId++;
        clearMapping();
    }
}

/**
 * @brief Remove Qubit with specific id
 *
 * @param id
 * @return true if succcessfully removed
 * @return false if not found or the qubit is not empty
 */
bool QCir::removeQubit(size_t id) {
    // Delete the ancilla only if whole line is empty
    QCirQubit *target = getQubit(id);
    if (target == NULL) {
        cerr << "Error: id " << id << " not found!!" << endl;
        return false;
    } else {
        if (target->getLast() != NULL || target->getFirst() != NULL) {
            cerr << "Error: id " << id << " is not an empty qubit!!" << endl;
            return false;
        } else {
            std::erase(_qubits, target);
            clearMapping();
            return true;
        }
    }
}

/**
 * @brief Add rotate-z gate and transform it to T, S, Z, Sdg, or Tdg if possible
 *
 * @param bit
 * @param phase
 * @param append if true, append the gate else prepend
 * @return QCirGate*
 */
QCirGate *QCir::addSingleRZ(size_t bit, Phase phase, bool append) {
    vector<size_t> qubit;
    qubit.push_back(bit);
    if (phase == Phase(1, 4))
        return addGate("t", qubit, phase, append);
    else if (phase == Phase(1, 2))
        return addGate("s", qubit, phase, append);
    else if (phase == Phase(1))
        return addGate("z", qubit, phase, append);
    else if (phase == Phase(3, 2))
        return addGate("sdg", qubit, phase, append);
    else if (phase == Phase(7, 4))
        return addGate("tdg", qubit, phase, append);
    else
        return addGate("rz", qubit, phase, append);
}

/**
 * @brief Add Gate
 *
 * @param type
 * @param bits
 * @param phase
 * @param append if true, append the gate, else prepend
 *
 * @return QCirGate*
 */
QCirGate *QCir::addGate(string type, vector<size_t> bits, Phase phase, bool append) {
    QCirGate *temp = NULL;
    for_each(type.begin(), type.end(), [](char &c) { c = ::tolower(c); });
    if (type == "h")
        temp = new HGate(_gateId);
    else if (type == "z")
        temp = new ZGate(_gateId);
    else if (type == "s")
        temp = new SGate(_gateId);
    else if (type == "s*" || type == "sdg" || type == "sd")
        temp = new SDGGate(_gateId);
    else if (type == "t")
        temp = new TGate(_gateId);
    else if (type == "tdg" || type == "td" || type == "t*")
        temp = new TDGGate(_gateId);
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
    else if (type == "ccx" || type == "ccnot")
        temp = new CCXGate(_gateId);
    else if (type == "cz")
        temp = new CZGate(_gateId);
    else if (type == "ccz")
        temp = new CCZGate(_gateId);
    // Note: rz and p has a little difference
    else if (type == "rz") {
        temp = new RZGate(_gateId);
        temp->setRotatePhase(phase);
    } else if (type == "rx") {
        temp = new RXGate(_gateId);
        temp->setRotatePhase(phase);
    } else if (type == "p") {
        temp = new PGate(_gateId);
        temp->setRotatePhase(phase);
    } else if (type == "px") {
        temp = new PXGate(_gateId);
        temp->setRotatePhase(phase);
    } else if (type == "mcp" || type == "cp") {
        temp = new MCPGate(_gateId);
        temp->setRotatePhase(phase);
    } else if (type == "mcpx" || type == "cpx") {
        temp = new MCPXGate(_gateId);
        temp->setRotatePhase(phase);
    } else if (type == "mcpy" || type == "cpy" || type == "py") {
        temp = new MCPYGate(_gateId);
        temp->setRotatePhase(phase);
    } else if (type == "mcrz" || type == "crz") {
        temp = new MCRZGate(_gateId);
        temp->setRotatePhase(phase);
    } else if (type == "mcrx" || type == "crx") {
        temp = new MCRXGate(_gateId);
        temp->setRotatePhase(phase);
    } else if (type == "mcry" || type == "cry" || type == "ry") {
        temp = new MCRYGate(_gateId);
        temp->setRotatePhase(phase);
    } else {
        cerr << "Error: The gate " << type << " is not implemented!!" << endl;
        abort();
        return nullptr;
    }
    if (append) {
        size_t max_time = 0;
        for (size_t k = 0; k < bits.size(); k++) {
            size_t q = bits[k];
            temp->addQubit(q, k == bits.size() - 1);  // target is the last one
            QCirQubit *target = getQubit(q);
            if (target->getLast() != NULL) {
                temp->setParent(q, target->getLast());
                target->getLast()->setChild(q, temp);
                if ((target->getLast()->getTime()) > max_time)
                    max_time = target->getLast()->getTime();
            } else {
                target->setFirst(temp);
            }
            target->setLast(temp);
        }
        temp->setTime(max_time + temp->getDelay());
    } else {
        for (size_t k = 0; k < bits.size(); k++) {
            size_t q = bits[k];
            temp->addQubit(q, k == bits.size() - 1);  // target is the last one
            QCirQubit *target = getQubit(q);
            if (target->getFirst() != NULL) {
                temp->setChild(q, target->getFirst());
                target->getFirst()->setParent(q, temp);
            } else {
                target->setLast(temp);
            }
            target->setFirst(temp);
        }
        _dirty = true;
    }
    _qgates.push_back(temp);
    _gateId++;
    clearMapping();
    return temp;
}

/**
 * @brief Remove gate
 *
 * @param id
 * @return true
 * @return false
 */
bool QCir::removeGate(size_t id) {
    QCirGate *target = getGate(id);
    if (target == NULL) {
        cerr << "Error: id " << id << " not found!!" << endl;
        return false;
    } else {
        vector<BitInfo> Info = target->getQubits();
        for (size_t i = 0; i < Info.size(); i++) {
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
        std::erase(_qgates, target);
        _dirty = true;
        clearMapping();
        return true;
    }
}

/**
 * @brief Analysis the quantum circuit and estimate the Clifford and T count
 *
 * @param detail if true, print the detail information
 */
// TODO - Analysis qasm is correct since no MC in it. Would fix MC in future.
void QCir::analysis(bool detail) {
    size_t clifford = 0;
    size_t tfamily = 0;
    size_t cxcnt = 0;
    size_t nct = 0;
    size_t h = 0;
    size_t rz = 0;
    size_t z = 0;
    size_t s = 0;
    size_t sdg = 0;
    size_t t = 0;
    size_t tdg = 0;
    size_t rx = 0;
    size_t x = 0;
    size_t sx = 0;
    size_t ry = 0;
    size_t y = 0;
    size_t sy = 0;

    size_t mcp = 0;
    size_t cz = 0;
    size_t ccz = 0;
    size_t crz = 0;
    size_t mcrx = 0;
    size_t cx = 0;
    size_t ccx = 0;
    size_t mcry = 0;

    auto analysisMCR = [&clifford, &tfamily, &nct, &cxcnt](QCirGate *g) -> void {
        if (g->getQubits().size() == 2) {
            if (g->getPhase().denominator() == 1) {
                clifford++;
                if (g->getType() != GateType::MCPX || g->getType() != GateType::MCRX) clifford += 2;
                cxcnt++;
            } else if (g->getPhase().denominator() == 2) {
                clifford += 2;
                cxcnt += 2;
                tfamily += 3;
            } else
                nct++;
        } else if (g->getQubits().size() == 1) {
            if (g->getPhase().denominator() <= 2)
                clifford++;
            else if (g->getPhase().denominator() == 4)
                tfamily++;
            else
                nct++;
        } else
            nct++;
    };

    for (auto &g : _qgates) {
        GateType type = g->getType();
        switch (type) {
            case GateType::H:
                h++;
                clifford++;
                break;
            case GateType::P:
                rz++;
                if (g->getPhase().denominator() <= 2)
                    clifford++;
                else if (g->getPhase().denominator() == 4)
                    tfamily++;
                else
                    nct++;
                break;
            case GateType::RZ:
                rz++;
                if (g->getPhase().denominator() <= 2)
                    clifford++;
                else if (g->getPhase().denominator() == 4)
                    tfamily++;
                else
                    nct++;
                break;
            case GateType::Z:
                z++;
                clifford++;
                break;
            case GateType::S:
                s++;
                clifford++;
                break;
            case GateType::SDG:
                sdg++;
                clifford++;
                break;
            case GateType::T:
                t++;
                tfamily++;
                break;
            case GateType::TDG:
                tdg++;
                tfamily++;
                break;
            case GateType::RX:
                rx++;
                if (g->getPhase().denominator() <= 2)
                    clifford++;
                else if (g->getPhase().denominator() == 4)
                    tfamily++;
                else
                    nct++;
                break;
            case GateType::X:
                x++;
                clifford++;
                break;
            case GateType::SX:
                sx++;
                clifford++;
                break;
            case GateType::RY:
                ry++;
                if (g->getPhase().denominator() <= 2)
                    clifford++;
                else if (g->getPhase().denominator() == 4)
                    tfamily++;
                else
                    nct++;
                break;
            case GateType::Y:
                y++;
                clifford++;
                break;
            case GateType::SY:
                sy++;
                clifford++;
                break;
            case GateType::MCP:
                mcp++;
                analysisMCR(g);
                break;
            case GateType::CZ:
                cz++;           // --C--
                clifford += 3;  // H-X-H
                cxcnt++;
                break;
            case GateType::CCZ:
                cz++;
                tfamily += 7;
                clifford += 10;
                cxcnt += 6;
                break;
            case GateType::MCRX:
                mcrx++;
                analysisMCR(g);
                break;
            case GateType::CX:
                cx++;
                clifford++;
                cxcnt++;
                break;
            case GateType::CCX:
                ccx++;
                tfamily += 7;
                clifford += 8;
                cxcnt += 6;
                break;
            case GateType::MCRY:
                mcry++;
                analysisMCR(g);
                break;
            default:
                cerr << "Error: the gate type is ERRORTYPE" << endl;
                break;
        }
    }
    size_t singleZ = rz + z + s + sdg + t + tdg;
    size_t singleX = rx + x + sx;
    size_t singleY = ry + y + sy;
    // cout << "───── Quantum Circuit Analysis ─────" << endl;
    // cout << endl;
    if (detail) {
        cout << "├── Single-qubit gate: " << h + singleZ + singleX + singleY << endl;
        cout << "│   ├── H: " << h << endl;
        cout << "│   ├── Z-family: " << singleZ << endl;
        cout << "│   │   ├── Z   : " << z << endl;
        cout << "│   │   ├── S   : " << s << endl;
        cout << "│   │   ├── S†  : " << sdg << endl;
        cout << "│   │   ├── T   : " << t << endl;
        cout << "│   │   ├── T†  : " << tdg << endl;
        cout << "│   │   └── RZ  : " << rz << endl;
        cout << "│   ├── X-family: " << singleX << endl;
        cout << "│   │   ├── X   : " << x << endl;
        cout << "│   │   ├── SX  : " << sx << endl;
        cout << "│   │   └── RX  : " << rx << endl;
        cout << "│   └── Y-family: " << singleY << endl;
        cout << "│       ├── Y   : " << y << endl;
        cout << "│       ├── SY  : " << sy << endl;
        cout << "│       └── RY  : " << ry << endl;
        cout << "└── Multiple-qubit gate: " << crz + mcp + cz + ccz + mcrx + cx + ccx + mcry << endl;
        cout << "    ├── Z-family: " << cz + ccz + crz + mcp << endl;
        cout << "    │   ├── CZ  : " << cz << endl;
        cout << "    │   ├── CCZ : " << ccz << endl;
        cout << "    │   ├── CRZ : " << crz << endl;
        cout << "    │   └── MCP : " << mcp << endl;
        cout << "    ├── X-family: " << cx + ccx + mcrx << endl;
        cout << "    │   ├── CX  : " << cx << endl;
        cout << "    │   ├── CCX : " << ccx << endl;
        cout << "    │   └── MCRX: " << mcrx << endl;
        cout << "    └── Y family: " << mcry << endl;
        cout << "        └── MCRY: " << mcry << endl;
        cout << endl;
    }
    // cout << "> Decompose into basic gate set" << endl;
    // cout << endl;
    cout << TF::BOLD(TF::GREEN("Clifford    : " + to_string(clifford))) << endl;
    cout << "└── " << TF::BOLD(TF::RED("2-qubit : " + to_string(cxcnt))) << endl;
    cout << TF::BOLD(TF::RED("T-family    : " + to_string(tfamily))) << endl;
    if (nct > 0)
        cout << TF::BOLD(TF::RED("Others      : " + to_string(nct))) << endl;
    else
        cout << TF::BOLD(TF::GREEN("Others      : " + to_string(nct))) << endl;
    // cout << endl;
}