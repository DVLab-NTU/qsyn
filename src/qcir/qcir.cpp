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
#include <tl/enumerate.hpp>
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

QCir::QCir(QCir const& other) {
    namespace views = std::ranges::views;
    this->add_qubits(other._qubits.size());
    for (size_t i = 0; i < _qubits.size(); i++) {
        _qubits[i]->set_id(other._qubits[i]->get_id());
    }

    for (auto& gate : other.get_gates()) {
        // We do not call add_gate here because we want to keep the original gate id
        _id_to_gates.emplace(gate->get_id(), std::make_unique<QCirGate>(gate->get_id(), gate->get_rotation_category(), gate->get_phase()));
        auto* new_gate = _id_to_gates.at(gate->get_id()).get();
        new_gate->set_qubits(gate->get_qubits());

        for (auto const qb : new_gate->get_qubits()) {
            QCirQubit* target = get_qubit(qb);
            if (target->get_last() != nullptr) {
                _connect(target->get_last()->get_id(), new_gate->get_id(), qb);
            } else {
                target->set_first(new_gate);
            }
            target->set_last(new_gate);
        }
    }
    _gate_id = other.get_gates().empty()
                   ? 0
                   : 1 + std::ranges::max(
                             other.get_gates() |
                             views::transform(
                                 [](QCirGate* g) { return g->get_id(); }));

    _qubit_id = other._qubits.empty()
                    ? 0
                    : 1 + std::ranges::max(
                              other._qubits |
                              views::transform(
                                  [](QCirQubit* qb) { return qb->get_id(); }));

    this->set_filename(other._filename);
    this->add_procedures(other._procedures);

    _dirty = true;
}
/**
 * @brief Get Gate.
 *
 * @param id : the gate id. Accepts an std::optional<size_t> for monadic chaining.
 * @return QCirGate*
 */
QCirGate* QCir::get_gate(std::optional<size_t> id) const {
    if (!id.has_value()) return nullptr;
    if (_id_to_gates.contains(*id)) {
        return _id_to_gates.at(*id).get();
    }
    return nullptr;
}

/**
 * @brief get the predecessors of a gate.
 *
 * @param gate_id : the gate id to query. Accepts an std::optional<size_t> for monadic chaining.
 * @param pin
 * @return std::optional<size_t>
 */
std::optional<size_t> QCir::get_predecessor(std::optional<size_t> gate_id, size_t pin) const {
    if (!gate_id.has_value()) return std::nullopt;
    if (get_gate(gate_id) == nullptr) {
        return std::nullopt;
    }
    if (pin >= get_gate(gate_id)->get_num_qubits()) {
        return std::nullopt;
    }
    auto prev = get_gate(gate_id)->legacy_get_qubits()[pin]._prev;
    return prev ? std::make_optional(prev->get_id()) : std::nullopt;
}

/**
 * @brief get the successors of a gate.
 *
 * @param gate_id : the gate id to query. Accepts an std::optional<size_t> for monadic chaining.
 * @param pin
 * @return std::optional<size_t>
 */
std::optional<size_t> QCir::get_successor(std::optional<size_t> gate_id, size_t pin) const {
    if (!gate_id.has_value()) return std::nullopt;
    if (get_gate(gate_id) == nullptr) {
        return std::nullopt;
    }
    if (pin >= get_gate(gate_id)->get_num_qubits()) {
        return std::nullopt;
    }
    auto next = get_gate(gate_id)->legacy_get_qubits()[pin]._next;
    return next ? std::make_optional(next->get_id()) : std::nullopt;
}

std::vector<std::optional<size_t>> QCir::get_predecessors(std::optional<size_t> gate_id) const {
    if (!gate_id.has_value()) return {};
    if (get_gate(gate_id) == nullptr) {
        return {};
    }
    return get_gate(gate_id)->legacy_get_qubits() |
           std::views::transform([](auto const& q) {
               return q._prev ? std::make_optional(q._prev->get_id()) : std::nullopt;
           }) |
           tl::to<std::vector>();
}

std::vector<std::optional<size_t>> QCir::get_successors(std::optional<size_t> gate_id) const {
    if (!gate_id.has_value()) return {};
    if (get_gate(gate_id) == nullptr) {
        return {};
    }
    return get_gate(gate_id)->legacy_get_qubits() |
           std::views::transform([](auto const& q) {
               return q._next ? std::make_optional(q._next->get_id()) : std::nullopt;
           }) |
           tl::to<std::vector>();
}

void QCir::_set_predecessor(size_t gate_id, size_t pin, std::optional<size_t> pred) const {
    if (get_gate(gate_id) == nullptr) return;
    if (pin >= get_gate(gate_id)->get_num_qubits()) return;
    if (!pred.has_value()) {
        get_gate(gate_id)->legacy_get_qubits()[pin]._prev = nullptr;
        return;
    }
    get_gate(gate_id)->legacy_get_qubits()[pin]._prev = get_gate(pred);
}

void QCir::_set_successor(size_t gate_id, size_t pin, std::optional<size_t> succ) const {
    if (get_gate(gate_id) == nullptr) return;
    if (pin >= get_gate(gate_id)->get_num_qubits()) return;
    if (!succ.has_value()) {
        get_gate(gate_id)->legacy_get_qubits()[pin]._next = nullptr;
        return;
    }
    get_gate(gate_id)->legacy_get_qubits()[pin]._next = get_gate(succ);
}

void QCir::_set_predecessors(size_t gate_id, std::vector<std::optional<size_t>> const& preds) const {
    if (get_gate(gate_id) == nullptr) return;
    if (preds.size() != get_gate(gate_id)->get_num_qubits()) return;
    for (size_t i = 0; i < preds.size(); i++) {
        get_gate(gate_id)->legacy_get_qubits()[i]._prev = preds[i].has_value() ? get_gate(preds[i]) : nullptr;
    }
}

void QCir::_set_successors(size_t gate_id, std::vector<std::optional<size_t>> const& succs) const {
    if (get_gate(gate_id) == nullptr) return;
    if (succs.size() != get_gate(gate_id)->get_num_qubits()) return;
    for (size_t i = 0; i < succs.size(); i++) {
        get_gate(gate_id)->legacy_get_qubits()[i]._next = succs[i].has_value() ? get_gate(succs[i]) : nullptr;
    }
}

void QCir::_connect(size_t gid1, size_t gid2, QubitIdType qubit) {
    if (get_gate(gid1) == nullptr || get_gate(gid2) == nullptr) return;
    auto pin1 = get_gate(gid1)->get_pin_by_qubit(qubit);
    auto pin2 = get_gate(gid2)->get_pin_by_qubit(qubit);
    if (pin1 == std::nullopt || pin2 == std::nullopt) return;

    _set_successor(gid1, *pin1, gid2);
    _set_predecessor(gid2, *pin2, gid1);
}
/**
 * @brief Get Qubit.
 *
 * @param id
 * @return QCirQubit
 */
QCirQubit* QCir::get_qubit(QubitIdType id) const {
    for (size_t i = 0; i < _qubits.size(); i++) {
        if (_qubits[i]->get_id() == id)
            return _qubits[i];
    }
    return nullptr;
}

size_t QCir::calculate_depth() const {
    if (is_empty()) return 0;
    auto const times = calculate_gate_times();
    return std::ranges::max(times | std::views::values);
}

std::unordered_map<size_t, size_t> QCir::calculate_gate_times() const {
    auto gate_times   = std::unordered_map<size_t, size_t>{};
    auto const lambda = [&](QCirGate* curr_gate) {
        auto const predecessors = get_predecessors(curr_gate->get_id());
        size_t const max_time   = std::ranges::max(
            predecessors |
            std::views::transform(
                [&](std::optional<size_t> predecessor) {
                    return predecessor ? gate_times.at(*predecessor) : 0;
                }));

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
QCirQubit* QCir::push_qubit() {
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
QCirQubit* QCir::insert_qubit(QubitIdType id) {
    assert(get_qubit(id) == nullptr);
    auto temp = new QCirQubit(id);
    auto cnt  = std::ranges::count_if(_qubits, [id](QCirQubit* q) { return q->get_id() < id; });

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
    QCirQubit* target = get_qubit(id);
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
QCirGate* QCir::add_gate(std::string type, QubitIdList const& bits, dvlab::Phase phase, bool append) {
    type           = dvlab::str::tolower_string(type);
    auto gate_type = str_to_gate_type(type);
    if (!gate_type.has_value()) {
        spdlog::error("Gate type {} is not supported!!", type);
        return nullptr;
    }
    auto const& [category, num_qubits, gate_phase] = gate_type.value();
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

QCirGate* QCir::append(GateType gate, QubitIdList const& bits) {
    _id_to_gates.emplace(_gate_id, std::make_unique<QCirGate>(_gate_id, std::get<0>(gate), *std::get<2>(gate)));
    auto* g = _id_to_gates[_gate_id].get();
    g->set_qubits(bits);
    _gate_id++;

    for (auto const& qb : g->get_qubits()) {
        QCirQubit* target = get_qubit(qb);
        if (target->get_last() != nullptr) {
            _connect(target->get_last()->get_id(), g->get_id(), qb);
        } else {
            target->set_first(g);
        }
        target->set_last(g);
    }
    _dirty = true;
    return g;
}

QCirGate* QCir::prepend(GateType gate, QubitIdList const& bits) {
    _id_to_gates.emplace(_gate_id, std::make_unique<QCirGate>(_gate_id, std::get<0>(gate), *std::get<2>(gate)));
    auto* g = _id_to_gates[_gate_id].get();
    g->set_qubits(bits);
    _gate_id++;

    for (auto const& qb : g->get_qubits()) {
        QCirQubit* target = get_qubit(qb);
        if (target->get_first() != nullptr) {
            _connect(g->get_id(), target->get_first()->get_id(), qb);
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
    QCirGate* target = get_gate(id);
    if (target == nullptr) {
        spdlog::error("Gate ID {} not found!!", id);
        return false;
    } else {
        for (size_t i = 0; i < target->get_num_qubits(); i++) {
            auto pred_id = get_predecessor(target->get_id(), i);
            auto succ_id = get_successor(target->get_id(), i);
            auto pred    = get_gate(pred_id);
            auto succ    = get_gate(succ_id);
            if (pred) {
                _set_successor(pred->get_id(), *pred->get_pin_by_qubit(target->get_qubit(i)), succ ? std::make_optional(succ->get_id()) : std::nullopt);
            } else {
                get_qubit(target->get_qubit(i))->set_first(succ);
            }
            if (succ) {
                _set_predecessor(succ->get_id(), *succ->get_pin_by_qubit(target->get_qubit(i)), pred ? std::make_optional(pred->get_id()) : std::nullopt);
            } else {
                get_qubit(target->get_qubit(i))->set_last(pred);
            }
        }

        _id_to_gates.erase(id);
        _dirty = true;
        return true;
    }
}

void add_input_cone_to(QCir const& qcir, QCirGate* gate, std::unordered_set<QCirGate*>& input_cone) {
    if (gate == nullptr) {
        return;
    }

    std::queue<size_t> queue;
    queue.push(gate->get_id());
    input_cone.insert(gate);

    while (!queue.empty()) {
        auto curr_gate = queue.front();
        queue.pop();

        for (auto const& predecessor : qcir.get_predecessors(curr_gate)) {
            if (predecessor.has_value() && !input_cone.contains(qcir.get_gate(predecessor))) {
                input_cone.insert(qcir.get_gate(predecessor));
                queue.push(*predecessor);
            }
        }
    }
}

void add_output_cone_to(QCir const& qcir, QCirGate* gate, std::unordered_set<QCirGate*>& output_cone) {
    if (gate == nullptr) {
        return;
    }

    std::queue<size_t> queue;
    queue.push(gate->get_id());
    output_cone.insert(gate);

    while (!queue.empty()) {
        auto curr_gate = queue.front();
        queue.pop();

        for (auto const& successor : qcir.get_successors(curr_gate)) {
            if (successor.has_value() && !output_cone.contains(qcir.get_gate(successor))) {
                output_cone.insert(qcir.get_gate(successor));
                queue.push(*successor);
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

    auto analysis_mcr = [&stat](QCirGate* g) -> void {
        if (g->legacy_get_qubits().size() == 2) {
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
        } else if (g->legacy_get_qubits().size() == 1) {
            if (g->get_phase().denominator() <= 2)
                stat.clifford++;
            else if (g->get_phase().denominator() == 4)
                stat.tfamily++;
            else
                stat.nct++;
        } else
            stat.nct++;
    };

    for (auto g : _id_to_gates | std::views::values | std::views::transform([](auto& p) { return p.get(); })) {
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

    auto const is_clifford = [](QCirGate* g) -> bool {
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

    std::unordered_set<QCirGate*> not_final, not_initial;

    for (auto const& g : get_gates()) {
        if (is_clifford(g)) continue;
        add_input_cone_to(*this, g, not_final);
        add_output_cone_to(*this, g, not_initial);
    }

    // the intersection of the two sets is the internal gates
    for (auto const& g : get_gates()) {
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

void QCir::translate(QCir const& qcir, std::string const& gate_set) {
    add_qubits(qcir.get_num_qubits());
    Equivalence equivalence = EQUIVALENCE_LIBRARY[gate_set];
    for (auto const* cur_gate : qcir.get_gates()) {
        std::string const type = cur_gate->get_type_str();

        if (!equivalence.contains(type)) {
            this->add_gate(type, cur_gate->get_qubits(),
                           cur_gate->get_phase(), true);
            continue;
        }

        for (auto const& [gate_type, gate_qubit_list, gate_phase] : equivalence[type]) {
            QubitIdList gate_qubit_id_list;
            for (auto qubit_num : gate_qubit_list) {
                gate_qubit_id_list.emplace_back(cur_gate->get_qubit(qubit_num));
            }
            this->add_gate(gate_type, gate_qubit_id_list, gate_phase, true);
        }
    }
    set_gate_set(gate_set);
}

}  // namespace qsyn::qcir
