/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define basic QCir functions ]
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
#include "qsyn/qsyn_type.hpp"
#include "util/phase.hpp"
#include "util/text_format.hpp"

namespace qsyn::qcir {

QCir::QCir(QCir const &other) {
    namespace views = std::ranges::views;
    other.update_topological_order();
    this->add_qubits(other._qubits.size());

    for (size_t i = 0; i < _qubits.size(); i++) {
        _qubits[i]->set_id(other._qubits[i]->get_id());
    }

    for (auto &gate : other._topological_order) {
        auto bit_range = gate->get_qubits() |
                         std::views::transform([](QubitInfo const &qb) { return qb._qubit; });
        auto new_gate = this->add_gate(
            gate->get_type_str(), {bit_range.begin(), bit_range.end()},
            gate->get_phase(), true);

        new_gate->set_id(gate->get_id());
    }

    this->_set_next_gate_id(1 + std::ranges::max(
                                    other._topological_order | views::transform(
                                                                   [](QCirGate *g) { return g->get_id(); })));
    this->_set_next_qubit_id(1 + std::ranges::max(
                                     other._qubits | views::transform(
                                                         [](QCirQubit *qb) { return qb->get_id(); })));
    this->set_filename(other._filename);
    this->add_procedures(other._procedures);
}
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

size_t QCir::get_depth() {
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
QCirQubit *QCir::insert_qubit(QubitIdType id) {
    assert(get_qubit(id) == nullptr);
    QCirQubit *temp = new QCirQubit(id);
    auto cnt        = std::ranges::count_if(_qubits, [id](QCirQubit *q) { return q->get_id() < id; });

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
bool QCir::remove_qubit(QubitIdType id) {
    // Delete the ancilla only if whole line is empty
    QCirQubit *target = get_qubit(id);
    if (target == nullptr) {
        std::cerr << "Error: id " << id << " not found!!" << std::endl;
        return false;
    } else {
        if (target->get_last() != nullptr || target->get_first() != nullptr) {
            std::cerr << "Error: id " << id << " is not an empty qubit!!" << std::endl;
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
QCirGate *QCir::add_single_rz(QubitIdType bit, dvlab::Phase phase, bool append) {
    QubitIdList qubit{bit};
    if (phase == dvlab::Phase(1, 4))
        return add_gate("t", qubit, phase, append);
    else if (phase == dvlab::Phase(1, 2))
        return add_gate("s", qubit, phase, append);
    else if (phase == dvlab::Phase(1))
        return add_gate("z", qubit, phase, append);
    else if (phase == dvlab::Phase(3, 2))
        return add_gate("sdg", qubit, phase, append);
    else if (phase == dvlab::Phase(7, 4))
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
QCirGate *QCir::add_gate(std::string type, QubitIdList bits, dvlab::Phase phase, bool append) {
    type           = dvlab::str::tolower_string(type);
    auto gate_type = str_to_gate_type(type);
    if (!gate_type.has_value()) {
        LOGGER.fatal("Gate type {} is not supported!!", type);
        abort();
    }
    auto const &[category, num_qubits, gate_phase] = gate_type.value();
    if (num_qubits.has_value() && num_qubits.value() != bits.size()) {
        LOGGER.fatal("Gate {} requires {} qubits, but {} qubits are given.", type, num_qubits.value(), bits.size());
        abort();
    }
    if (gate_phase.has_value()) {
        phase = gate_phase.value();
    }
    QCirGate *temp = new QCirGate(_gate_id, category, phase);

    if (append) {
        size_t max_time = 0;
        for (size_t k = 0; k < bits.size(); k++) {
            auto q = bits[k];
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
            auto q = bits[k];
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
        std::cerr << "Error: id " << id << " not found!!" << std::endl;
        return false;
    } else {
        std::vector<QubitInfo> info = target->get_qubits();
        for (size_t i = 0; i < info.size(); i++) {
            if (info[i]._prev != nullptr)
                info[i]._prev->set_child(info[i]._qubit, info[i]._next);
            else
                get_qubit(info[i]._qubit)->set_first(info[i]._next);
            if (info[i]._next != nullptr)
                info[i]._next->set_parent(info[i]._qubit, info[i]._prev);
            else
                get_qubit(info[i]._qubit)->set_last(info[i]._prev);
            info[i]._prev = nullptr;
            info[i]._next = nullptr;
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
    size_t tfamily  = 0;
    size_t cxcnt    = 0;
    size_t nct      = 0;
    size_t h        = 0;
    size_t rz       = 0;
    size_t z        = 0;
    size_t s        = 0;
    size_t sdg      = 0;
    size_t t        = 0;
    size_t tdg      = 0;
    size_t rx       = 0;
    size_t x        = 0;
    size_t sx       = 0;
    size_t ry       = 0;
    size_t y        = 0;
    size_t sy       = 0;

    size_t mcpz = 0;
    size_t cz   = 0;
    size_t ccz  = 0;
    size_t crz  = 0;
    size_t mcrx = 0;
    size_t cx   = 0;
    size_t ccx  = 0;
    size_t mcry = 0;

    auto analysis_mcr = [&clifford, &tfamily, &nct, &cxcnt](QCirGate *g) -> void {
        if (g->get_qubits().size() == 2) {
            if (g->get_phase().denominator() == 1) {
                clifford++;
                if (g->get_rotation_category() != GateRotationCategory::px || g->get_rotation_category() != GateRotationCategory::rx) clifford += 2;
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
        GateRotationCategory type = g->get_rotation_category();
        switch (type) {
            case GateRotationCategory::h:
                h++;
                clifford++;
                break;
            case GateRotationCategory::pz:
            case GateRotationCategory::rz:
                if (get_num_qubits() == 1) {
                    rz++;
                    if (g->get_phase() == dvlab::Phase(1)) {
                        z++;
                    }
                    if (g->get_phase() == dvlab::Phase(1, 2)) {
                        s++;
                    }
                    if (g->get_phase() == dvlab::Phase(-1, 2)) {
                        sdg++;
                    }
                    if (g->get_phase() == dvlab::Phase(1, 4)) {
                        t++;
                    }
                    if (g->get_phase() == dvlab::Phase(-1, 4)) {
                        tdg++;
                    }
                    if (g->get_phase().denominator() <= 2)
                        clifford++;
                    else if (g->get_phase().denominator() == 4)
                        tfamily++;
                    else
                        nct++;
                } else if (g->get_num_qubits() == 2) {
                    cz++;           // --C--
                    clifford += 3;  // H-X-H
                    cxcnt++;
                } else if (g->get_num_qubits() == 3) {
                    ccz++;
                    tfamily += 7;
                    clifford += 10;
                    cxcnt += 6;
                } else {
                    mcpz++;
                    analysis_mcr(g);
                }
                break;
            case GateRotationCategory::px:
            case GateRotationCategory::rx:
                if (g->get_num_qubits() == 1) {
                    rx++;
                    if (g->get_phase() == dvlab::Phase(1)) {
                        x++;
                    }
                    if (g->get_phase() == dvlab::Phase(1, 2)) {
                        sx++;
                    }
                    if (g->get_phase().denominator() <= 2)
                        clifford++;
                    else if (g->get_phase().denominator() == 4)
                        tfamily++;
                    else
                        nct++;
                } else if (g->get_num_qubits() == 2) {
                    cx++;
                    clifford++;
                    cxcnt++;
                } else if (g->get_num_qubits() == 3) {
                    ccx++;
                    tfamily += 7;
                    clifford += 8;
                    cxcnt += 6;
                } else {
                    mcrx++;
                    analysis_mcr(g);
                }
                break;
            case GateRotationCategory::py:
            case GateRotationCategory::ry:
                if (g->get_num_qubits() == 1) {
                    ry++;
                    if (g->get_phase() == dvlab::Phase(1)) {
                        y++;
                    }
                    if (g->get_phase() == dvlab::Phase(1, 2)) {
                        sy++;
                    }
                    if (g->get_phase().denominator() <= 2)
                        clifford++;
                    else if (g->get_phase().denominator() == 4)
                        tfamily++;
                    else
                        nct++;
                } else {
                    mcry++;
                    analysis_mcr(g);
                }
                break;
            // case GateCategory::mcpz:
            //     if (g->get_num_qubits() == 2) {
            //         cz++;           // --C--
            //         clifford += 3;  // H-X-H
            //         cxcnt++;
            //     } else if (g->get_num_qubits() == 3) {
            //         ccz++;
            //         tfamily += 7;
            //         clifford += 10;
            //         cxcnt += 6;
            //     } else {
            //         mcpz++;
            //         analysis_mcr(g);
            //     }
            //     break;
            default:
                LOGGER.fatal("Gate {} is not supported!!", g->get_type_str());
                abort();
        }
    }
    size_t single_z = rz + z + s + sdg + t + tdg;
    size_t single_x = rx + x + sx;
    size_t single_y = ry + y + sy;
    // cout << "───── Quantum Circuit Analysis ─────" << endl;
    // cout << endl;
    if (detail) {
        std::cout << "├── Single-qubit gate: " << h + single_z + single_x + single_y << std::endl;
        std::cout << "│   ├── H: " << h << std::endl;
        std::cout << "│   ├── Z-family: " << single_z << std::endl;
        std::cout << "│   │   ├── Z   : " << z << std::endl;
        std::cout << "│   │   ├── S   : " << s << std::endl;
        std::cout << "│   │   ├── S†  : " << sdg << std::endl;
        std::cout << "│   │   ├── T   : " << t << std::endl;
        std::cout << "│   │   ├── T†  : " << tdg << std::endl;
        std::cout << "│   │   └── RZ  : " << rz << std::endl;
        std::cout << "│   ├── X-family: " << single_x << std::endl;
        std::cout << "│   │   ├── X   : " << x << std::endl;
        std::cout << "│   │   ├── SX  : " << sx << std::endl;
        std::cout << "│   │   └── RX  : " << rx << std::endl;
        std::cout << "│   └── Y-family: " << single_y << std::endl;
        std::cout << "│       ├── Y   : " << y << std::endl;
        std::cout << "│       ├── SY  : " << sy << std::endl;
        std::cout << "│       └── RY  : " << ry << std::endl;
        std::cout << "└── Multiple-qubit gate: " << crz + mcpz + cz + ccz + mcrx + cx + ccx + mcry << std::endl;
        std::cout << "    ├── Z-family: " << cz + ccz + crz + mcpz << std::endl;
        std::cout << "    │   ├── CZ  : " << cz << std::endl;
        std::cout << "    │   ├── CCZ : " << ccz << std::endl;
        std::cout << "    │   ├── CRZ : " << crz << std::endl;
        std::cout << "    │   └── MCP : " << mcpz << std::endl;
        std::cout << "    ├── X-family: " << cx + ccx + mcrx << std::endl;
        std::cout << "    │   ├── CX  : " << cx << std::endl;
        std::cout << "    │   ├── CCX : " << ccx << std::endl;
        std::cout << "    │   └── MCRX: " << mcrx << std::endl;
        std::cout << "    └── Y family: " << mcry << std::endl;
        std::cout << "        └── MCRY: " << mcry << std::endl;
        std::cout << std::endl;
    }
    if (print) {
        using namespace dvlab;
        fmt::println("Clifford    : {}", fmt_ext::styled_if_ansi_supported(clifford, fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
        fmt::println("└── 2-qubit : {}", fmt_ext::styled_if_ansi_supported(cxcnt, fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
        fmt::println("T-family    : {}", fmt_ext::styled_if_ansi_supported(tfamily, fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
        fmt::println("Others      : {}", fmt_ext::styled_if_ansi_supported(nct, fmt::fg((nct > 0) ? fmt::terminal_color::red : fmt::terminal_color::green) | fmt::emphasis::bold));
    }
    std::vector<int> info;
    info.emplace_back(clifford);
    info.emplace_back(cxcnt);
    info.emplace_back(tfamily);
    return info;  // [clifford, cxcnt, tfamily]
}

}  // namespace qsyn::qcir