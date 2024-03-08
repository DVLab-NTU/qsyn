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
#include <iterator>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

#include "./gate_type.hpp"
#include "./qcir_gate.hpp"
#include "./qcir_qubit.hpp"
#include "./qcir_translate.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/phase.hpp"
#include "util/scope_guard.hpp"
#include "util/text_format.hpp"
#include "util/util.hpp"

namespace qsyn::qcir {

QCir::QCir(QCir const &other) {
    namespace views = std::ranges::views;
    this->add_qubits(other._qubits.size());
    for (size_t i = 0; i < _qubits.size(); i++) {
        _qubits[i]->set_id(other._qubits[i]->get_id());
    }

    for (auto &gate : other.get_gates()) {
        auto new_gate = this->add_gate(
            gate->get_type_str(), gate->get_operands(),
            gate->get_phase(), true);

        new_gate->set_id(gate->get_id());
    }
    _gate_id = other.get_gates().empty()
                   ? 0
                   : 1 + std::ranges::max(
                             other.get_gates() |
                             views::transform(
                                 [](QCirGate *g) { return g->get_id(); }));

    _qubit_id = other._qubits.empty()
                    ? 0
                    : 1 + std::ranges::max(
                              other._qubits |
                              views::transform(
                                  [](QCirQubit *qb) { return qb->get_id(); }));

    this->set_filename(other._filename);
    this->add_procedures(other._procedures);

    _dirty = true;
}
/**
 * @brief Get Gate.
 *
 * @param id
 * @return QCirGate*
 */
QCirGate *QCir::get_gate(size_t id) const {
    if (_id_to_gates.contains(id)) {
        return _id_to_gates.at(id).get();
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
    return std::ranges::max(calculate_gate_times() | std::views::values);
}

std::unordered_map<size_t, size_t> QCir::calculate_gate_times() const {
    auto gate_times   = std::unordered_map<size_t, size_t>{};
    auto const lambda = [&](QCirGate *curr_gate) {
        std::vector<QubitInfo> info = curr_gate->get_qubits();
        size_t max_time             = 0;
        for (size_t i = 0; i < info.size(); i++) {
            if (info[i]._prev == nullptr)
                continue;
            if (gate_times.at(info[i]._prev->get_id()) > max_time)
                max_time = gate_times.at(info[i]._prev->get_id());
        }
        gate_times.emplace(curr_gate->get_id(), max_time + curr_gate->get_delay());
    };

    std::ranges::for_each(get_gates(), lambda);

    return gate_times;
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
 * @brief Add Gate
 *
 * @param type
 * @param bits
 * @param phase
 * @param append if true, append the gate, else prepend
 *
 * @return QCirGate*
 */
QCirGate *QCir::add_gate(std::string type, QubitIdList const &bits, dvlab::Phase phase, bool append) {
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

    return append
               ? this->append(std::make_tuple(category, bits.size(), phase), bits)
               : this->prepend(std::make_tuple(category, bits.size(), phase), bits);
}

QCirGate *QCir::append(GateType gate, QubitIdList const &bits) {
    _id_to_gates.emplace(_gate_id, std::make_unique<QCirGate>(_gate_id, std::get<0>(gate), *std::get<2>(gate)));
    auto *g = _id_to_gates[_gate_id].get();
    g->set_operands(bits);
    _gate_id++;

    for (auto const &qb : g->get_operands()) {
        QCirQubit *target = get_qubit(qb);
        if (target->get_last() != nullptr) {
            g->set_parent(qb, target->get_last());
            target->get_last()->set_child(qb, g);
        } else {
            target->set_first(g);
        }
        target->set_last(g);
    }
    _dirty = true;
    return g;
}

QCirGate *QCir::prepend(GateType gate, QubitIdList const &bits) {
    _id_to_gates.emplace(_gate_id, std::make_unique<QCirGate>(_gate_id, std::get<0>(gate), *std::get<2>(gate)));
    auto *g = _id_to_gates[_gate_id].get();
    g->set_operands(bits);

    _gate_id++;

    for (auto const &qb : g->get_operands()) {
        QCirQubit *target = get_qubit(qb);
        if (target->get_first() != nullptr) {
            g->set_child(qb, target->get_first());
            target->get_first()->set_parent(qb, g);
        } else {
            target->set_last(g);
        }
        target->set_first(g);
    }
    _dirty = true;
    return g;
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
                info[i]._prev->set_child(target->get_operand(i), info[i]._next);
            else
                get_qubit(target->get_operand(i))->set_first(info[i]._next);
            if (info[i]._next != nullptr)
                info[i]._next->set_parent(target->get_operand(i), info[i]._prev);
            else
                get_qubit(target->get_operand(i))->set_last(info[i]._prev);
            info[i]._prev = nullptr;
            info[i]._next = nullptr;
        }
        _id_to_gates.erase(id);
        _dirty = true;
        return true;
    }
}

void add_input_cone_to(QCirGate *gate, std::unordered_set<QCirGate *> &input_cone) {
    if (gate == nullptr) {
        return;
    }

    std::queue<QCirGate *> q;
    q.push(gate);
    input_cone.insert(gate);

    while (!q.empty()) {
        QCirGate *curr_gate = q.front();
        q.pop();

        for (const auto &qubit_info : curr_gate->get_qubits()) {
            QCirGate *parent_gate = qubit_info._prev;
            if (parent_gate != nullptr && !input_cone.contains(parent_gate)) {
                input_cone.insert(parent_gate);
                q.push(parent_gate);
            }
        }
    }
}

void add_output_cone_to(QCirGate *gate, std::unordered_set<QCirGate *> &output_cone) {
    if (gate == nullptr) {
        return;
    }

    std::queue<QCirGate *> q;
    q.push(gate);
    output_cone.insert(gate);

    while (!q.empty()) {
        QCirGate *curr_gate = q.front();
        q.pop();

        for (const auto &qubit_info : curr_gate->get_qubits()) {
            QCirGate *child_gate = qubit_info._next;
            if (child_gate != nullptr && !output_cone.contains(child_gate)) {
                output_cone.insert(child_gate);
                q.push(child_gate);
            }
        }
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

    for (auto g : _id_to_gates | std::views::values | std::views::transform([](auto &p) { return p.get(); })) {
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
            case GateRotationCategory::ecr:
                stat.ecr++;
                stat.twoqubit++;
                break;
            default:
                DVLAB_ASSERT(false, fmt::format("Gate {} is not supported!!", g->get_type_str()));
        }
    }

    auto const is_clifford = [](QCirGate *g) -> bool {
        using GRC       = GateRotationCategory;
        auto const type = g->get_rotation_category();
        switch (type) {
            case GRC::id:
            case GRC::h:
            case GRC::swap:
            case GRC::ecr:
                return true;
            case GRC::pz:
            case GRC::rz:
            case GRC::px:
            case GRC::rx:
            case GRC::py:
            case GRC::ry:
                if (g->get_num_qubits() == 1) {
                    return g->get_phase().denominator() <= 2;
                } else if (g->get_num_qubits() == 2) {
                    return g->get_phase().denominator() == 1;
                } else {
                    return false;
                }
        }
        DVLAB_UNREACHABLE("Every rotation category should be handled in the switch-case");
    };

    std::unordered_set<QCirGate *> not_final, not_initial;

    for (auto const &g : get_gates()) {
        if (is_clifford(g)) continue;
        add_input_cone_to(g, not_final);
        add_output_cone_to(g, not_initial);
    }

    // the intersection of the two sets is the internal gates
    for (auto const &g : get_gates()) {
        if (g->get_type_str() == "h" && not_final.contains(g) && not_initial.contains(g)) {
            stat.h_internal++;
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
    fmt::println("├── H-gate  : {} ({} internal)", fmt_ext::styled_if_ansi_supported(stat.h, fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold), stat.h_internal);
    fmt::println("└── 2-qubit : {}", fmt_ext::styled_if_ansi_supported(stat.twoqubit, fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
    fmt::println("T-family    : {}", fmt_ext::styled_if_ansi_supported(stat.tfamily, fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
    fmt::println("Others      : {}", fmt_ext::styled_if_ansi_supported(stat.nct, fmt::fg((stat.nct > 0) ? fmt::terminal_color::red : fmt::terminal_color::green) | fmt::emphasis::bold));
}

void QCir::translate(QCir const &qcir, std::string const &gate_set) {
    add_qubits(qcir.get_num_qubits());
    Equivalence equivalence = EQUIVALENCE_LIBRARY[gate_set];
    for (auto const *cur_gate : qcir.get_gates()) {
        std::string const type = cur_gate->get_type_str();

        if (!equivalence.contains(type)) {
            this->add_gate(type, cur_gate->get_operands(),
                           cur_gate->get_phase(), true);
            continue;
        }

        for (auto const &[gate_type, gate_qubit_list, gate_phase] : equivalence[type]) {
            QubitIdList gate_qubit_id_list;
            for (auto qubit_num : gate_qubit_list) {
                gate_qubit_id_list.emplace_back(cur_gate->get_operand(qubit_num));
            }
            this->add_gate(gate_type, gate_qubit_id_list, gate_phase, true);
        }
    }
    set_gate_set(gate_set);
}

}  // namespace qsyn::qcir
