/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define basic QCir functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>

#include "./qcir_gate.hpp"
#include "./qcir_qubit.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/phase.hpp"
#include "util/text_format.hpp"
#include "util/util.hpp"

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
    if (other._qgates.size() > 0) {
        this->_set_next_gate_id(1 + std::ranges::max(
                                        other._qgates | views::transform(
                                                            [](QCirGate *g) { return g->get_id(); })));
    } else {
        this->_set_next_gate_id(0);
    }

    if (other._qubits.size() > 0) {
        this->_set_next_qubit_id(1 + std::ranges::max(
                                         other._qubits | views::transform(
                                                             [](QCirQubit *qb) { return qb->get_id(); })));
    } else {
        this->_set_next_qubit_id(0);
    }

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
QCirQubit *QCir::get_qubit(QubitIdType id) const {
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]->get_id() == id)
            return _qubits[i];
    }
    return nullptr;
}

size_t QCir::calculate_depth() const {
    if (is_empty()) return 0;
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
    auto temp = new QCirQubit(_qubit_id);
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
    auto temp = new QCirQubit(id);
    auto cnt  = std::ranges::count_if(_qubits, [id](QCirQubit *q) { return q->get_id() < id; });

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
        spdlog::error("Qubit ID {} not found!!", id);
        return false;
    } else {
        if (target->get_last() != nullptr || target->get_first() != nullptr) {
            spdlog::error("Qubit ID {} is not empty!!", id);
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
    auto qubit = QubitIdList{bit};
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
        spdlog::error("Gate type {} is not supported!!", type);
        return nullptr;
    }
    auto const &[category, num_qubits, gate_phase] = gate_type.value();
    if (num_qubits.has_value() && num_qubits.value() != bits.size()) {
        spdlog::error("Gate {} requires {} qubits, but {} qubits are given.", type, num_qubits.value(), bits.size());
        return nullptr;
    }
    if (gate_phase.has_value()) {
        phase = gate_phase.value();
    }
    auto temp = new QCirGate(_gate_id, category, phase);

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
        spdlog::error("Gate ID {} not found!!", id);
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
// FIXME - Analysis qasm is correct since no MC in it. Would fix MC in future.
QCirGateStatistics QCir::get_gate_statistics() const {
    auto stat = QCirGateStatistics{};
    if (is_empty()) return stat;

    auto analysis_mcr = [&stat](QCirGate *g) -> void {
        if (g->get_qubits().size() == 2) {
            if (g->get_phase().denominator() == 1) {
                stat.clifford++;
                if (g->get_rotation_category() != GateRotationCategory::px || g->get_rotation_category() != GateRotationCategory::rx) stat.clifford += 2;
                stat.twoqubit++;
            } else if (g->get_phase().denominator() == 2) {
                stat.clifford += 2;
                stat.twoqubit += 2;
                stat.tfamily += 3;
            } else
                stat.nct++;
        } else if (g->get_qubits().size() == 1) {
            if (g->get_phase().denominator() <= 2)
                stat.clifford++;
            else if (g->get_phase().denominator() == 4)
                stat.tfamily++;
            else
                stat.nct++;
        } else
            stat.nct++;
    };

    for (auto &g : _qgates) {
        auto type = g->get_rotation_category();
        switch (type) {
            case GateRotationCategory::h:
                stat.h++;
                stat.clifford++;
                break;
            case GateRotationCategory::swap:
                stat.clifford += 3;
                stat.twoqubit += 3;
                stat.cx += 3;
                break;
            case GateRotationCategory::pz:
            case GateRotationCategory::rz:
                if (get_num_qubits() == 1) {
                    stat.rz++;
                    if (g->get_phase() == dvlab::Phase(1)) {
                        stat.z++;
                    }
                    if (g->get_phase() == dvlab::Phase(1, 2)) {
                        stat.s++;
                    }
                    if (g->get_phase() == dvlab::Phase(-1, 2)) {
                        stat.sdg++;
                    }
                    if (g->get_phase() == dvlab::Phase(1, 4)) {
                        stat.t++;
                    }
                    if (g->get_phase() == dvlab::Phase(-1, 4)) {
                        stat.tdg++;
                    }
                    if (g->get_phase().denominator() <= 2)
                        stat.clifford++;
                    else if (g->get_phase().denominator() == 4)
                        stat.tfamily++;
                    else
                        stat.nct++;
                } else if (g->get_num_qubits() == 2) {
                    stat.cz++;           // --C--
                    stat.clifford += 3;  // H-X-H
                    stat.twoqubit++;
                } else if (g->get_num_qubits() == 3) {
                    stat.ccz++;
                    stat.tfamily += 7;
                    stat.clifford += 10;
                    stat.twoqubit += 6;
                } else {
                    stat.mcpz++;
                    analysis_mcr(g);
                }
                break;
            case GateRotationCategory::px:
            case GateRotationCategory::rx:
                if (g->get_num_qubits() == 1) {
                    stat.rx++;
                    if (g->get_phase() == dvlab::Phase(1)) {
                        stat.x++;
                    }
                    if (g->get_phase() == dvlab::Phase(1, 2)) {
                        stat.sx++;
                    }
                    if (g->get_phase().denominator() <= 2)
                        stat.clifford++;
                    else if (g->get_phase().denominator() == 4)
                        stat.tfamily++;
                    else
                        stat.nct++;
                } else if (g->get_num_qubits() == 2) {
                    stat.cx++;
                    stat.clifford++;
                    stat.twoqubit++;
                } else if (g->get_num_qubits() == 3) {
                    stat.ccx++;
                    stat.tfamily += 7;
                    stat.clifford += 8;
                    stat.twoqubit += 6;
                } else {
                    stat.mcrx++;
                    analysis_mcr(g);
                }
                break;
            case GateRotationCategory::py:
            case GateRotationCategory::ry:
                if (g->get_num_qubits() == 1) {
                    stat.ry++;
                    if (g->get_phase() == dvlab::Phase(1)) {
                        stat.y++;
                    }
                    if (g->get_phase() == dvlab::Phase(1, 2)) {
                        stat.sy++;
                    }
                    if (g->get_phase().denominator() <= 2)
                        stat.clifford++;
                    else if (g->get_phase().denominator() == 4)
                        stat.tfamily++;
                    else
                        stat.nct++;
                } else {
                    stat.mcry++;
                    analysis_mcr(g);
                }
                break;
            default:
                DVLAB_ASSERT(false, fmt::format("Gate {} is not supported!!", g->get_type_str()));
        }
    }

    return stat;
}
void QCir::print_gate_statistics(bool detail) const {
    using namespace dvlab;
    if (is_empty()) return;
    auto stat = get_gate_statistics();

    auto const single_z = stat.rz + stat.z + stat.s + stat.sdg + stat.t + stat.tdg;
    auto const single_x = stat.rx + stat.x + stat.sx;
    auto const single_y = stat.ry + stat.y + stat.sy;
    if (detail) {
        fmt::println("├── Single-qubit gate: {}", stat.h + single_z + single_x + single_y);
        fmt::println("│   ├── H: {}", stat.h);
        fmt::println("│   ├── Z-family: {}", single_z);
        fmt::println("│   │   ├── Z   : {}", stat.z);
        fmt::println("│   │   ├── S   : {}", stat.s);
        fmt::println("│   │   ├── S†  : {}", stat.sdg);
        fmt::println("│   │   ├── T   : {}", stat.t);
        fmt::println("│   │   ├── T†  : {}", stat.tdg);
        fmt::println("│   │   └── RZ  : {}", stat.rz);
        fmt::println("│   ├── X-family: {}", single_x);
        fmt::println("│   │   ├── X   : {}", stat.x);
        fmt::println("│   │   ├── SX  : {}", stat.sx);
        fmt::println("│   │   └── RX  : {}", stat.rx);
        fmt::println("│   └── Y family: {}", single_y);
        fmt::println("│       ├── Y   : {}", stat.y);
        fmt::println("│       ├── SY  : {}", stat.sy);
        fmt::println("│       └── RY  : {}", stat.ry);
        fmt::println("└── Multiple-qubit gate: {}", stat.mcpz + stat.cz + stat.ccz + stat.mcrx + stat.cx + stat.ccx + stat.mcry);
        fmt::println("    ├── Z-family: {}", stat.cz + stat.ccz + stat.mcpz);
        fmt::println("    │   ├── CZ  : {}", stat.cz);
        fmt::println("    │   ├── CCZ : {}", stat.ccz);
        fmt::println("    │   └── MCP : {}", stat.mcpz);
        fmt::println("    ├── X-family: {}", stat.cx + stat.ccx + stat.mcrx);
        fmt::println("    │   ├── CX  : {}", stat.cx);
        fmt::println("    │   ├── CCX : {}", stat.ccx);
        fmt::println("    │   └── MCRX: {}", stat.mcrx);
        fmt::println("    └── Y family: {}", stat.mcry);
        fmt::println("        └── MCRY: {}", stat.mcry);
        fmt::println("");
    }

    fmt::println("Clifford    : {}", fmt_ext::styled_if_ansi_supported(stat.clifford, fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
    fmt::println("└── 2-qubit : {}", fmt_ext::styled_if_ansi_supported(stat.twoqubit, fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
    fmt::println("T-family    : {}", fmt_ext::styled_if_ansi_supported(stat.tfamily, fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
    fmt::println("Others      : {}", fmt_ext::styled_if_ansi_supported(stat.nct, fmt::fg((stat.nct > 0) ? fmt::terminal_color::red : fmt::terminal_color::green) | fmt::emphasis::bold));
}

}  // namespace qsyn::qcir
