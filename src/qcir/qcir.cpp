/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Edition functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir.hpp"

#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>

#include "./qcir_gate.hpp"
#include "./qcir_qubit.hpp"
#include "util/text_format.hpp"

using namespace std;

/**
 * @brief Get Gate.
 *
 * @param id
 * @return QCirGate*
 */
QCirGate *QCir::get_gate(size_t id) const {
    for (size_t i = 0; i < _qgates.size(); i++) {
        if (_qgates[i]->get_id() == id)
            return _qgates[i];
    }
    return nullptr;
}

/**
 * @brief Get Qubit.
 *
 * @param id
 * @return QCirQubit
 */
QCirQubit *QCir::get_qubit(size_t id) const {
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]->get_id() == id)
            return _qubits[i];
    }
    return nullptr;
}

int QCir::get_depth() {
    if (_dirty) update_gate_time();
    _dirty = false;
    return std::ranges::max(_qgates | std::views::transform([](QCirGate *qg) { return qg->get_time(); }));
}

/**
 * @brief Add single Qubit.
 *
 * @return QCirQubit*
 */
QCirQubit *QCir::push_qubit() {
    QCirQubit *temp = new QCirQubit(_qubit_id);
    _qubits.emplace_back(temp);
    _qubit_id++;
    return temp;
}

/**
 * @brief Insert single Qubit.
 *
 * @param id
 * @return QCirQubit*
 */
QCirQubit *QCir::insert_qubit(size_t id) {
    assert(get_qubit(id) == nullptr);
    QCirQubit *temp = new QCirQubit(id);
    size_t cnt = 0;
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]->get_id() < id)
            cnt++;
        else
            break;
    }
    _qubits.insert(_qubits.begin() + cnt, temp);
    return temp;
}

/**
 * @brief Add Qubit.
 *
 * @param num
 */
void QCir::add_qubits(size_t num) {
    for (size_t i = 0; i < num; i++) {
        _qubits.emplace_back(new QCirQubit(_qubit_id));
        _qubit_id++;
    }
}

/**
 * @brief Remove Qubit with specific id
 *
 * @param id
 * @return true if succcessfully removed
 * @return false if not found or the qubit is not empty
 */
bool QCir::remove_qubit(size_t id) {
    // Delete the ancilla only if whole line is empty
    QCirQubit *target = get_qubit(id);
    if (target == nullptr) {
        cerr << "Error: id " << id << " not found!!" << endl;
        return false;
    } else {
        if (target->get_last() != nullptr || target->get_first() != nullptr) {
            cerr << "Error: id " << id << " is not an empty qubit!!" << endl;
            return false;
        } else {
            std::erase(_qubits, target);
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
QCirGate *QCir::add_single_rz(size_t bit, Phase phase, bool append) {
    vector<size_t> qubit;
    qubit.emplace_back(bit);
    if (phase == Phase(1, 4))
        return add_gate("t", qubit, phase, append);
    else if (phase == Phase(1, 2))
        return add_gate("s", qubit, phase, append);
    else if (phase == Phase(1))
        return add_gate("z", qubit, phase, append);
    else if (phase == Phase(3, 2))
        return add_gate("sdg", qubit, phase, append);
    else if (phase == Phase(7, 4))
        return add_gate("tdg", qubit, phase, append);
    else
        return add_gate("rz", qubit, phase, append);
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
QCirGate *QCir::add_gate(string type, vector<size_t> bits, Phase phase, bool append) {
    QCirGate *temp = nullptr;
    type = dvlab::str::to_lower_string(type);
    if (type == "h")
        temp = new HGate(_gate_id);
    else if (type == "z")
        temp = new ZGate(_gate_id);
    else if (type == "s")
        temp = new SGate(_gate_id);
    else if (type == "s*" || type == "sdg" || type == "sd")
        temp = new SDGGate(_gate_id);
    else if (type == "t")
        temp = new TGate(_gate_id);
    else if (type == "tdg" || type == "td" || type == "t*")
        temp = new TDGGate(_gate_id);
    else if (type == "x" || type == "not")
        temp = new XGate(_gate_id);
    else if (type == "y")
        temp = new YGate(_gate_id);
    else if (type == "sx" || type == "x_1_2")
        temp = new SXGate(_gate_id);
    else if (type == "sy" || type == "y_1_2")
        temp = new SYGate(_gate_id);
    else if (type == "cx" || type == "cnot")
        temp = new CXGate(_gate_id);
    else if (type == "ccx" || type == "ccnot")
        temp = new CCXGate(_gate_id);
    else if (type == "cz")
        temp = new CZGate(_gate_id);
    else if (type == "ccz")
        temp = new CCZGate(_gate_id);
    // Note: rz and p has a little difference
    else if (type == "rz") {
        temp = new RZGate(_gate_id);
        temp->set_phase(phase);
    } else if (type == "rx") {
        temp = new RXGate(_gate_id);
        temp->set_phase(phase);
    } else if (type == "p" || type == "pz") {
        temp = new PGate(_gate_id);
        temp->set_phase(phase);
    } else if (type == "px") {
        temp = new PXGate(_gate_id);
        temp->set_phase(phase);
    } else if (type == "mcp" || type == "mcpz" || type == "cp") {
        temp = new MCPGate(_gate_id);
        temp->set_phase(phase);
    } else if (type == "mcpx" || type == "cpx") {
        temp = new MCPXGate(_gate_id);
        temp->set_phase(phase);
    } else if (type == "mcpy" || type == "cpy" || type == "py") {
        temp = new MCPYGate(_gate_id);
        temp->set_phase(phase);
    } else if (type == "mcrz" || type == "crz") {
        temp = new MCRZGate(_gate_id);
        temp->set_phase(phase);
    } else if (type == "mcrx" || type == "crx") {
        temp = new MCRXGate(_gate_id);
        temp->set_phase(phase);
    } else if (type == "mcry" || type == "cry" || type == "ry") {
        temp = new MCRYGate(_gate_id);
        temp->set_phase(phase);
    } else {
        cerr << "Error: the gate " << type << " is not implemented!!" << endl;
        abort();
        return nullptr;
    }
    if (append) {
        size_t max_time = 0;
        for (size_t k = 0; k < bits.size(); k++) {
            size_t q = bits[k];
            temp->add_qubit(q, k == bits.size() - 1);  // target is the last one
            QCirQubit *target = get_qubit(q);
            if (target->get_last() != nullptr) {
                temp->set_parent(q, target->get_last());
                target->get_last()->set_child(q, temp);
                if ((target->get_last()->get_time()) > max_time)
                    max_time = target->get_last()->get_time();
            } else {
                target->set_first(temp);
            }
            target->set_last(temp);
        }
        temp->set_time(max_time + temp->get_delay());
    } else {
        for (size_t k = 0; k < bits.size(); k++) {
            size_t q = bits[k];
            temp->add_qubit(q, k == bits.size() - 1);  // target is the last one
            QCirQubit *target = get_qubit(q);
            if (target->get_first() != nullptr) {
                temp->set_child(q, target->get_first());
                target->get_first()->set_parent(q, temp);
            } else {
                target->set_last(temp);
            }
            target->set_first(temp);
        }
        _dirty = true;
    }
    _qgates.emplace_back(temp);
    _gate_id++;
    return temp;
}

/**
 * @brief Remove gate
 *
 * @param id
 * @return true
 * @return false
 */
bool QCir::remove_gate(size_t id) {
    QCirGate *target = get_gate(id);
    if (target == nullptr) {
        cerr << "Error: id " << id << " not found!!" << endl;
        return false;
    } else {
        vector<QubitInfo> info = target->get_qubits();
        for (size_t i = 0; i < info.size(); i++) {
            if (info[i]._parent != nullptr)
                info[i]._parent->set_child(info[i]._qubit, info[i]._child);
            else
                get_qubit(info[i]._qubit)->set_first(info[i]._child);
            if (info[i]._child != nullptr)
                info[i]._child->set_parent(info[i]._qubit, info[i]._parent);
            else
                get_qubit(info[i]._qubit)->set_last(info[i]._parent);
            info[i]._parent = nullptr;
            info[i]._child = nullptr;
        }
        std::erase(_qgates, target);
        _dirty = true;
        return true;
    }
}

/**
 * @brief Analysis the quantum circuit and estimate the Clifford and T count
 *
 * @param detail if true, print the detail information
 */
// TODO - Analysis qasm is correct since no MC in it. Would fix MC in future.
std::vector<int> QCir::count_gates(bool detail, bool print) {
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

    auto analysis_mcr = [&clifford, &tfamily, &nct, &cxcnt](QCirGate *g) -> void {
        if (g->get_qubits().size() == 2) {
            if (g->get_phase().denominator() == 1) {
                clifford++;
                if (g->get_type() != GateType::mcpx || g->get_type() != GateType::mcrx) clifford += 2;
                cxcnt++;
            } else if (g->get_phase().denominator() == 2) {
                clifford += 2;
                cxcnt += 2;
                tfamily += 3;
            } else
                nct++;
        } else if (g->get_qubits().size() == 1) {
            if (g->get_phase().denominator() <= 2)
                clifford++;
            else if (g->get_phase().denominator() == 4)
                tfamily++;
            else
                nct++;
        } else
            nct++;
    };

    for (auto &g : _qgates) {
        GateType type = g->get_type();
        switch (type) {
            case GateType::h:
                h++;
                clifford++;
                break;
            case GateType::p:
                rz++;
                if (g->get_phase().denominator() <= 2)
                    clifford++;
                else if (g->get_phase().denominator() == 4)
                    tfamily++;
                else
                    nct++;
                break;
            case GateType::rz:
                rz++;
                if (g->get_phase().denominator() <= 2)
                    clifford++;
                else if (g->get_phase().denominator() == 4)
                    tfamily++;
                else
                    nct++;
                break;
            case GateType::z:
                z++;
                clifford++;
                break;
            case GateType::s:
                s++;
                clifford++;
                break;
            case GateType::sdg:
                sdg++;
                clifford++;
                break;
            case GateType::t:
                t++;
                tfamily++;
                break;
            case GateType::tdg:
                tdg++;
                tfamily++;
                break;
            case GateType::rx:
                rx++;
                if (g->get_phase().denominator() <= 2)
                    clifford++;
                else if (g->get_phase().denominator() == 4)
                    tfamily++;
                else
                    nct++;
                break;
            case GateType::x:
                x++;
                clifford++;
                break;
            case GateType::sx:
                sx++;
                clifford++;
                break;
            case GateType::ry:
                ry++;
                if (g->get_phase().denominator() <= 2)
                    clifford++;
                else if (g->get_phase().denominator() == 4)
                    tfamily++;
                else
                    nct++;
                break;
            case GateType::y:
                y++;
                clifford++;
                break;
            case GateType::sy:
                sy++;
                clifford++;
                break;
            case GateType::mcp:
                mcp++;
                analysis_mcr(g);
                break;
            case GateType::cz:
                cz++;           // --C--
                clifford += 3;  // H-X-H
                cxcnt++;
                break;
            case GateType::ccz:
                cz++;
                tfamily += 7;
                clifford += 10;
                cxcnt += 6;
                break;
            case GateType::mcrx:
                mcrx++;
                analysis_mcr(g);
                break;
            case GateType::cx:
                cx++;
                clifford++;
                cxcnt++;
                break;
            case GateType::ccx:
                ccx++;
                tfamily += 7;
                clifford += 8;
                cxcnt += 6;
                break;
            case GateType::mcry:
            default:
                mcry++;
                analysis_mcr(g);
                break;
        }
    }
    size_t single_z = rz + z + s + sdg + t + tdg;
    size_t single_x = rx + x + sx;
    size_t single_y = ry + y + sy;
    // cout << "───── Quantum Circuit Analysis ─────" << endl;
    // cout << endl;
    if (detail) {
        cout << "├── Single-qubit gate: " << h + single_z + single_x + single_y << endl;
        cout << "│   ├── H: " << h << endl;
        cout << "│   ├── Z-family: " << single_z << endl;
        cout << "│   │   ├── Z   : " << z << endl;
        cout << "│   │   ├── S   : " << s << endl;
        cout << "│   │   ├── S†  : " << sdg << endl;
        cout << "│   │   ├── T   : " << t << endl;
        cout << "│   │   ├── T†  : " << tdg << endl;
        cout << "│   │   └── RZ  : " << rz << endl;
        cout << "│   ├── X-family: " << single_x << endl;
        cout << "│   │   ├── X   : " << x << endl;
        cout << "│   │   ├── SX  : " << sx << endl;
        cout << "│   │   └── RX  : " << rx << endl;
        cout << "│   └── Y-family: " << single_y << endl;
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
    if (print) {
        using namespace dvlab;
        fmt::println("Clifford    : {}", fmt_ext::styled_if_ansi_supported(clifford, fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
        fmt::println("└── 2-qubit : {}", fmt_ext::styled_if_ansi_supported(cxcnt, fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
        fmt::println("T-family    : {}", fmt_ext::styled_if_ansi_supported(tfamily, fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
        fmt::println("Others      : {}", fmt_ext::styled_if_ansi_supported(nct, fmt::fg((nct > 0) ? fmt::terminal_color::red : fmt::terminal_color::green) | fmt::emphasis::bold));
    }
    vector<int> info;
    info.emplace_back(clifford);
    info.emplace_back(cxcnt);
    info.emplace_back(tfamily);
    return info;  // [clifford, cxcnt, tfamily]
}
