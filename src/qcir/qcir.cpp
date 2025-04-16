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
#include <variant>
#include <vector>

#include "./basic_gate_type.hpp"
#include "./qcir_gate.hpp"
#include "./qcir_qubit.hpp"
#include "./qcir_translate.hpp"
#include "convert/qcir_to_tableau.hpp"
#include "qcir/operation.hpp"
#include "qsyn/qsyn_type.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau_optimization.hpp"
#include "util/scope_guard.hpp"
#include "util/text_format.hpp"
#include "util/util.hpp"

namespace qsyn {
// these definitions are here to enable qcir::QCir as an Operation
template <>
std::optional<zx::ZXGraph> to_zxgraph(qcir::QCir const& qcir);
template <>
std::optional<qsyn::tensor::QTensor<double>> to_tensor(qcir::QCir const& qcir);
}  // namespace qsyn

namespace qsyn::qcir {

QCir::QCir(QCir const& other) {
    namespace views = std::ranges::views;
    this->add_qubits(other._qubits.size());

    for (auto& gate : other.get_gates()) {
        // We do not call append here because we want to keep the original gate id
        _id_to_gates.emplace(gate->get_id(), std::make_unique<QCirGate>(gate->get_id(), gate->get_operation(), gate->get_qubits()));
        _predecessors.emplace(gate->get_id(), std::vector<std::optional<size_t>>(gate->get_num_qubits(), std::nullopt));
        _successors.emplace(gate->get_id(), std::vector<std::optional<size_t>>(gate->get_num_qubits(), std::nullopt));
        auto* new_gate = _id_to_gates.at(gate->get_id()).get();

        for (auto const qb : new_gate->get_qubits()) {
            DVLAB_ASSERT(qb < get_num_qubits(), fmt::format("Qubit {} not found!!", qb));
            if (_qubits[qb].get_last_gate() != nullptr) {
                _connect(_qubits[qb].get_last_gate()->get_id(), new_gate->get_id(), qb);
            } else {
                _qubits[qb].set_first_gate(new_gate);
            }
            _qubits[qb].set_last_gate(new_gate);
        }
    }
    _gate_id = other.get_gates().empty()
                   ? 0
                   : 1 + std::ranges::max(
                             other.get_gates() |
                             views::transform(
                                 [](QCirGate* g) { return g->get_id(); }));

    this->set_filename(other._filename);
    this->add_procedures(other._procedures);
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

        gate_times.emplace(curr_gate->get_id(), max_time + 1);
    };

    std::ranges::for_each(get_gates(), lambda);

    return gate_times;
}

/**
 * @brief Insert single Qubit.
 *
 * @param id
 * @return QCirQubit*
 */
void QCir::insert_qubit(QubitIdType id) {
    if (id > _qubits.size()) {
        spdlog::error("Qubit ID {} is out of range!!", id);
        return;
    }
    _qubits.insert(dvlab::iterator::next(_qubits.begin(), id), QCirQubit{});

    for (auto& gate : get_gates()) {
        auto new_qubits = gate->get_qubits();
        for (auto& qubit : new_qubits) {
            if (qubit >= id) {
                qubit++;
            }
        }
        gate->set_qubits(new_qubits);
    }
}

/**
 * @brief Add Qubit.
 *
 * @param num
 */
void QCir::add_qubits(size_t num) {
    for (size_t i = 0; i < num; i++) {
        _qubits.emplace_back();
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
    if (id >= _qubits.size()) {
        spdlog::error("Qubit ID {} not found!!", id);
        return false;
    }
    if (_qubits[id].get_last_gate() != nullptr || _qubits[id].get_first_gate() != nullptr) {
        spdlog::error("Qubit ID {} is not empty!!", id);
        return false;
    }

    _qubits.erase(dvlab::iterator::next(_qubits.begin(), id));

    for (auto& gate : get_gates()) {
        auto new_qubits = gate->get_qubits();
        DVLAB_ASSERT(std::ranges::none_of(new_qubits, [id](auto q) { return q == id; }), fmt::format("Qubit {} is not empty!!", id));

        for (auto& qubit : new_qubits) {
            if (qubit > id) {
                qubit--;
            }
        }

        gate->set_qubits(new_qubits);
    }
    return true;
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
        DVLAB_ASSERT(qb < _qubits.size(), fmt::format("Qubit {} not found!!", qb));
        if (_qubits[qb].get_last_gate() != nullptr) {
            _connect(_qubits[qb].get_last_gate()->get_id(), g->get_id(), qb);
        } else {
            _qubits[qb].set_first_gate(g);
        }
        _qubits[qb].set_last_gate(g);
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
        DVLAB_ASSERT(qb < _qubits.size(), fmt::format("Qubit {} not found!!", qb));
        if (_qubits[qb].get_first_gate() != nullptr) {
            _connect(g->get_id(), _qubits[qb].get_first_gate()->get_id(), qb);
        } else {
            _qubits[qb].set_last_gate(g);
        }
        _qubits[qb].set_first_gate(g);
    }
    _dirty = true;
    return g->get_id();
}

size_t QCir::append(QCirGate const& gate) {
    return append(gate.get_operation(), gate.get_qubits());
}

size_t QCir::prepend(QCirGate const& gate) {
    return prepend(gate.get_operation(), gate.get_qubits());
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
                _qubits[target->get_qubit(i)].set_first_gate(succ);
            }
            if (succ) {
                _set_predecessor(succ->get_id(), *succ->get_pin_by_qubit(target->get_qubit(i)), pred ? std::make_optional(pred->get_id()) : std::nullopt);
            } else {
                _qubits[target->get_qubit(i)].set_last_gate(pred);
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
    std::unordered_map<std::string, size_t> gate_counts;

    // default types

    gate_counts.emplace("clifford", 0);
    gate_counts.emplace("1-qubit", 0);
    gate_counts.emplace("2-qubit", 0);
    gate_counts.emplace("t-family", 0);

    if (qcir.is_empty())
        return gate_counts;

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
        if (g->get_num_qubits() == 1) {
            gate_counts["1-qubit"]++;
        }
        if (g->get_num_qubits() == 2) {
            gate_counts["2-qubit"]++;
        }
        if (auto inner = g->get_operation().get_underlying_if<PXGate>(); inner && inner->get_phase().denominator() == 4) {
            gate_counts["t-family"]++;
        }
        if (auto inner = g->get_operation().get_underlying_if<PYGate>(); inner && inner->get_phase().denominator() == 4) {
            gate_counts["t-family"]++;
        }
        if (auto inner = g->get_operation().get_underlying_if<PZGate>(); inner && inner->get_phase().denominator() == 4) {
            gate_counts["t-family"]++;
        }
        if (auto inner = g->get_operation().get_underlying_if<RXGate>(); inner && inner->get_phase().denominator() == 4) {
            gate_counts["t-family"]++;
        }
        if (auto inner = g->get_operation().get_underlying_if<RYGate>(); inner && inner->get_phase().denominator() == 4) {
            gate_counts["t-family"]++;
        }
        if (auto inner = g->get_operation().get_underlying_if<RZGate>(); inner && inner->get_phase().denominator() == 4) {
            gate_counts["t-family"]++;
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
    auto const t_family   = stat.contains("t-family") ? stat.at("t-family") : 0;

    auto const other = get_num_gates() - clifford - t_family;

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

/**
 * @brief Get the first gate at the qubit
 *
 * @param qubit
 * @return QCirGate*
 */
QCirGate*
QCir::get_first_gate(QubitIdType qubit) const {
    return _qubits[qubit].get_first_gate();
}

/**
 * @brief Get the last gate at the qubit
 *
 * @param qubit
 * @return QCirGate*
 */
QCirGate*
QCir::get_last_gate(QubitIdType qubit) const {
    return _qubits[qubit].get_last_gate();
}

bool is_clifford(qcir::QCir const& qcir) {
    auto tabl = experimental::to_tableau(qcir);
    if (!tabl.has_value()) {
        return false;
    }
    experimental::collapse(*tabl);

    return (tabl->size() == 1) && std::holds_alternative<experimental::StabilizerTableau>(tabl->front());
}

Operation adjoint(qcir::QCir const& qcir) {
    auto copy = qcir;
    copy.adjoint_inplace();
    return copy;
}

std::optional<QCir> to_basic_gates(QCirGate const& gate) {
    auto qcir = to_basic_gates(gate.get_operation());
    if (!qcir.has_value()) {
        return std::nullopt;
    }
    auto const& gate_qubits = gate.get_qubits();
    for (auto& g : qcir->get_gates()) {
        // copy to circumvent g++ 11.4 compiler bug
        auto const curr_qubits = g->get_qubits();
        auto new_qubits =
            curr_qubits |
            std::views::transform([&](auto q) { return gate_qubits[q]; }) |
            tl::to<std::vector>();
        g->set_qubits(std::move(new_qubits));
    }
    return qcir;
}

template <>
// NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name)
std::optional<QCir> to_basic_gates(qcir::QCir const& qcir) {
    auto new_qcir = QCir{qcir.get_num_qubits()};
    for (auto const& g : qcir.get_gates()) {
        auto sub_qcir = to_basic_gates(*g);
        if (!sub_qcir.has_value()) {
            return std::nullopt;
        }
        for (auto const& sub_g : sub_qcir->get_gates()) {
            new_qcir.append(sub_g->get_operation(), sub_g->get_qubits());
        }
    }

    return new_qcir;
}

QCir as_qcir(Operation const& op) {
    auto const n_qubits = op.get_num_qubits();
    auto const qubits =
        std::views::iota(0ul, n_qubits) |
        tl::to<QubitIdList>();
    QCir qcir{n_qubits};
    qcir.append(op, qubits);
    return qcir;
}

}  // namespace qsyn::qcir
