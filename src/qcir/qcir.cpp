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
#include <queue>
#include <string>
#include <tl/enumerate.hpp>
#include <tl/fold.hpp>
#include <tl/to.hpp>
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
        // We do not call append here because we want to keep the original gate id
        _id_to_gates.emplace(gate->get_id(), std::make_unique<QCirGate>(gate->get_id(), gate->get_operation(), gate->get_qubits()));
        _predecessors.emplace(gate->get_id(), std::vector<std::optional<size_t>>(gate->get_num_qubits(), std::nullopt));
        _successors.emplace(gate->get_id(), std::vector<std::optional<size_t>>(gate->get_num_qubits(), std::nullopt));
        auto* new_gate = _id_to_gates.at(gate->get_id()).get();

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
    if (!_id_to_gates.contains(*gate_id)) {
        return std::nullopt;
    }
    if (pin >= get_gate(gate_id)->get_num_qubits()) {
        return std::nullopt;
    }
    assert(_predecessors.contains(*gate_id));
    return _predecessors.at(*gate_id)[pin];
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
    if (!_id_to_gates.contains(*gate_id)) {
        return std::nullopt;
    }
    if (pin >= get_gate(gate_id)->get_num_qubits()) {
        return std::nullopt;
    }
    assert(_successors.contains(*gate_id));
    return _successors.at(*gate_id)[pin];
}

std::vector<std::optional<size_t>> QCir::get_predecessors(std::optional<size_t> gate_id) const {
    if (!gate_id.has_value()) return {};
    if (!_id_to_gates.contains(*gate_id)) {
        return {};
    }
    assert(_predecessors.contains(*gate_id));
    return _predecessors.at(*gate_id);
}

std::vector<std::optional<size_t>> QCir::get_successors(std::optional<size_t> gate_id) const {
    if (!gate_id.has_value()) return {};
    if (!_id_to_gates.contains(*gate_id)) {
        return {};
    }
    assert(_successors.contains(*gate_id));
    return _successors.at(*gate_id);
}

void QCir::_set_predecessor(size_t gate_id, size_t pin, std::optional<size_t> pred) {
    if (!_id_to_gates.contains(gate_id)) return;
    if (pin >= get_gate(gate_id)->get_num_qubits()) return;
    assert(_predecessors.contains(gate_id));
    _predecessors.at(gate_id)[pin] = pred;
}

void QCir::_set_successor(size_t gate_id, size_t pin, std::optional<size_t> succ) {
    if (!_id_to_gates.contains(gate_id)) return;
    if (pin >= get_gate(gate_id)->get_num_qubits()) return;
    assert(_successors.contains(gate_id));
    _successors.at(gate_id)[pin] = succ;
}

void QCir::_set_predecessors(size_t gate_id, std::vector<std::optional<size_t>> const& preds) {
    if (!_id_to_gates.contains(gate_id)) return;
    if (preds.size() != get_gate(gate_id)->get_num_qubits()) return;
    assert(_predecessors.contains(gate_id));
    _predecessors.at(gate_id) = preds;
}

void QCir::_set_successors(size_t gate_id, std::vector<std::optional<size_t>> const& succs) {
    if (!_id_to_gates.contains(gate_id)) return;
    if (succs.size() != get_gate(gate_id)->get_num_qubits()) return;
    assert(_successors.contains(gate_id));
    _successors.at(gate_id) = succs;
}

void QCir::_connect(size_t gid1, size_t gid2, QubitIdType qubit) {
    if (!_id_to_gates.contains(gid1) || !_id_to_gates.contains(gid2)) return;
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
 * @return true if successfully removed
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
    auto const& [category, num_qubits, gate_phase] = *gate_type;
    if (num_qubits.has_value() && num_qubits.value() != bits.size()) {
        spdlog::error("Gate {} requires {} qubits, but {} qubits are given.", type, num_qubits.value(), bits.size());
        return nullptr;
    }
    if (gate_phase.has_value()) {
        phase = gate_phase.value();
    }

    return get_gate(
        append
            ? this->append(LegacyGateType(std::make_tuple(category, bits.size(), phase)), bits)
            : this->prepend(LegacyGateType(std::make_tuple(category, bits.size(), phase)), bits));
}

size_t QCir::append(Operation const& op, QubitIdList const& bits) {
    DVLAB_ASSERT(
        op.get_num_qubits() == bits.size(),
        fmt::format("Operation {} requires {} qubits, but {} qubits are given.", op.get_repr(), op.get_num_qubits(), bits.size()));
    _id_to_gates.emplace(_gate_id, std::make_unique<QCirGate>(_gate_id, op, bits));
    _predecessors.emplace(_gate_id, std::vector<std::optional<size_t>>(bits.size(), std::nullopt));
    _successors.emplace(_gate_id, std::vector<std::optional<size_t>>(bits.size(), std::nullopt));
    assert(_id_to_gates.contains(_gate_id));
    assert(_predecessors.contains(_gate_id));
    assert(_successors.contains(_gate_id));
    auto* g = _id_to_gates[_gate_id].get();
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
    return g->get_id();
}

size_t QCir::prepend(Operation const& op, QubitIdList const& bits) {
    DVLAB_ASSERT(
        op.get_num_qubits() == bits.size(),
        fmt::format("Operation {} requires {} qubits, but {} qubits are given.", op.get_repr(), op.get_num_qubits(), bits.size()));
    _id_to_gates.emplace(_gate_id, std::make_unique<QCirGate>(_gate_id, op, bits));
    _predecessors.emplace(_gate_id, std::vector<std::optional<size_t>>(bits.size(), std::nullopt));
    _successors.emplace(_gate_id, std::vector<std::optional<size_t>>(bits.size(), std::nullopt));
    assert(_id_to_gates.contains(_gate_id));
    assert(_predecessors.contains(_gate_id));
    assert(_successors.contains(_gate_id));
    auto* g = _id_to_gates[_gate_id].get();
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
    return g->get_id();
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
std::unordered_map<std::string, size_t> get_gate_statistics(QCir const& qcir) {
    if (qcir.is_empty()) return {};

    std::unordered_map<std::string, size_t> gate_counts;

    // default types

    gate_counts.emplace("clifford", 0);
    gate_counts.emplace("1-qubit", 0);
    gate_counts.emplace("2-qubit", 0);
    gate_counts.emplace("t-family", 0);

    for (auto g : qcir.get_gates()) {
        auto type = g->get_operation().get_repr();
        // strip params
        if (type.find('(') != std::string::npos) {
            auto pos = type.find('(');
            type     = type.substr(0, pos);
        }
        if (gate_counts.contains(type)) {
            gate_counts[type]++;
        } else {
            gate_counts[type] = 1;
        }
        if (is_clifford(g->get_operation())) {
            gate_counts["clifford"]++;
        }
        if (g->get_num_qubits() == 2) {
            gate_counts["2-qubit"]++;
        }
    }

    std::unordered_set<QCirGate*> not_final, not_initial;

    for (auto const& g : qcir.get_gates()) {
        if (is_clifford(g->get_operation())) continue;
        add_input_cone_to(qcir, g, not_final);
        add_output_cone_to(qcir, g, not_initial);
    }

    // the intersection of the two sets is the internal gates

    auto internal_h_count = std::ranges::count_if(
        qcir.get_gates(),
        [&](QCirGate* g) {
            return g->get_operation() == HGate() && not_final.contains(g) && not_initial.contains(g);
        });
    if (internal_h_count > 0) {
        gate_counts["h-internal"] = internal_h_count;
    }

    return gate_counts;
}
void QCir::print_gate_statistics(bool detail) const {
    using namespace dvlab;
    if (is_empty()) return;
    auto stat = get_gate_statistics(*this);

    auto const clifford   = stat.contains("clifford") ? stat.at("clifford") : 0;
    auto const two_qubit  = stat.contains("2-qubit") ? stat.at("2-qubit") : 0;
    auto const h          = stat.contains("h") ? stat.at("h") : 0;
    auto const h_internal = stat.contains("h-internal") ? stat.at("h-internal") : 0;
    auto const t_family =
        (stat.contains("t") ? stat.at("t") : 0) +
        (stat.contains("tdg") ? stat.at("tdg") : 0) +
        (stat.contains("tx") ? stat.at("tx") : 0) +
        (stat.contains("txdg") ? stat.at("txdg") : 0) +
        (stat.contains("ty") ? stat.at("ty") : 0) +
        (stat.contains("tydg") ? stat.at("tydg") : 0);

    auto const other = tl::fold_left(stat | std::views::values, 0, std::plus<>()) - clifford - t_family;

    if (detail) {
        auto const type_width  = std::ranges::max(stat | std::views::keys | std::views::transform([](std::string const& s) { return s.size(); }));
        auto const count_width = std::to_string(std::ranges::max(stat | std::views::values)).size();
        for (auto const& [type, count] : stat) {
            fmt::println("{0:<{1}} : {2:{3}}", type, type_width, count, count_width);
        }
        fmt::println("");
    }

    fmt::println("Clifford   : {}", fmt_ext::styled_if_ansi_supported(clifford, fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
    fmt::println("└── H-gate : {} ({} internal)", fmt_ext::styled_if_ansi_supported(h, fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold), h_internal);
    fmt::println("2-qubit    : {}", fmt_ext::styled_if_ansi_supported(two_qubit, fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
    fmt::println("T-family   : {}", fmt_ext::styled_if_ansi_supported(t_family, fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
    fmt::println("Others     : {}", fmt_ext::styled_if_ansi_supported(other, fmt::fg((other > 0) ? fmt::terminal_color::red : fmt::terminal_color::green) | fmt::emphasis::bold));
}

void QCir::translate(QCir const& qcir, std::string const& gate_set) {
    add_qubits(qcir.get_num_qubits());
    Equivalence equivalence = EQUIVALENCE_LIBRARY[gate_set];
    for (auto const* cur_gate : qcir.get_gates()) {
        std::string const type = cur_gate->get_type_str();

        if (!equivalence.contains(type)) {
            this->append(cur_gate->get_operation(), cur_gate->get_qubits());
            continue;
        }

        for (auto const& [gate_type, gate_qubit_list, gate_phase] : equivalence[type]) {
            QubitIdList gate_qubit_id_list;
            for (auto qubit_num : gate_qubit_list) {
                gate_qubit_id_list.emplace_back(cur_gate->get_qubit(qubit_num));
            }
            if (auto op = str_to_operation(gate_type); op.has_value())
                this->append(*op, gate_qubit_id_list);
        }
    }
    set_gate_set(gate_set);
}

void QCir::append(QCir const& other, std::map<QubitIdType, QubitIdType> const& qubit_map) {
    if (qubit_map.empty() && _qubits.size() != other.get_qubits().size()) {
        throw std::runtime_error("Qcir::append: you must provide a qubit map if the circuits are of different sizes.");
        return;
    }

    auto map_qubit = [&](QubitIdType id) -> QubitIdType {
        if (qubit_map.empty()) {
            return id;
        } else {
            return qubit_map.at(id);
        }
    };

    auto const& other_gates = other.get_gates();
    for (auto const& gate : other_gates) {
        auto const& qubits = gate->get_qubits();
        auto const& bits   = qubits |
                           std::views::transform([&](auto const& qb) { return map_qubit(qb); }) |
                           tl::to<QubitIdList>();
        append(gate->get_operation(), bits);
    }
}

}  // namespace qsyn::qcir
