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
    this->add_classical_bits(other._classical_bits.size());
    
    // Copy classical bit states (but not measurement gates - will be re-established)
    for (size_t i = 0; i < other._classical_bits.size(); ++i) {
        if (other._classical_bits[i].has_value()) {
            _classical_bits[i].set_value(other._classical_bits[i].get_value());
        }
    }

    for (auto& gate : other.get_gates()) {
        // We do not call append here because we want to keep the original gate id
        // Need to preserve classical bit information from gates
        if (gate->has_classical_bits()) {
            auto const& classical_bits = gate->get_classical_bits();
            auto const classical_value = gate->get_classical_value();
            
            if (gate->get_operation().get_type() == "measure" && classical_bits.size() == 1) {
                // Measurement gate
                _id_to_gates.emplace(gate->get_id(), std::make_unique<QCirGate>(gate->get_id(), gate->get_operation(), gate->get_qubits(), classical_bits));
            } else if (classical_value.has_value()) {
                // If-else gate
                if (classical_bits.size() == 1) {
                    _id_to_gates.emplace(gate->get_id(), std::make_unique<QCirGate>(gate->get_id(), gate->get_operation(), gate->get_qubits(), classical_bits[0], *classical_value));
                } else {
                    _id_to_gates.emplace(gate->get_id(), std::make_unique<QCirGate>(gate->get_id(), gate->get_operation(), gate->get_qubits(), *classical_value));
                }
            } else {
        _id_to_gates.emplace(gate->get_id(), std::make_unique<QCirGate>(gate->get_id(), gate->get_operation(), gate->get_qubits()));
            }
        } else {
            _id_to_gates.emplace(gate->get_id(), std::make_unique<QCirGate>(gate->get_id(), gate->get_operation(), gate->get_qubits()));
        }
        
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
        
        // Re-establish classical bit measurement linkage
        if (new_gate->get_operation().get_type() == "measure" && new_gate->has_classical_bits()) {
            auto const& classical_bits = new_gate->get_classical_bits();
            if (!classical_bits.empty()) {
                _classical_bits[classical_bits[0]].set_measured(new_gate);
            }
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

void QCir::_connect_classical(size_t measurement_gate_id, size_t if_else_gate_id, QubitIdType measurement_qubit, QubitIdType if_else_qubit) {
    if (!_id_to_gates.contains(measurement_gate_id) || !_id_to_gates.contains(if_else_gate_id)) {
        return;
    }
    
    auto* measurement_gate = get_gate(measurement_gate_id);
    auto* if_else_gate = get_gate(if_else_gate_id);
    
    if (!measurement_gate || !if_else_gate) {
        return;
    }
    
    // Get the pins for both qubits
    auto pin1 = measurement_gate->get_pin_by_qubit(measurement_qubit);
    auto pin2 = if_else_gate->get_pin_by_qubit(if_else_qubit);
    
    if (pin1 == std::nullopt || pin2 == std::nullopt) {
        spdlog::error("Qubit {} not found in measurement gate {} or qubit {} not found in if-else gate {}", 
                     measurement_qubit, measurement_gate_id, if_else_qubit, if_else_gate_id);
        return;
    }
    
    // For classical dependencies, we need to add extra pins beyond the qubit pins
    // Extend the successors vector for the measurement gate to accommodate classical dependencies
    _successors[measurement_gate_id].push_back(if_else_gate_id);
    
    // Extend the predecessors vector for the if-else gate
    _predecessors[if_else_gate_id].push_back(measurement_gate_id);
}


//------------------------------------------------------------------------
//   Validation methods
//------------------------------------------------------------------------

/**
 * @brief Validate that qubits can have gates added (not measured)
 * 
 * @param qubits List of qubits to check
 * @param gate_type Type of gate being added
 * @return true if valid, false otherwise
 */
bool QCir::_validate_qubit_gate_addition(QubitIdList const& qubits, std::string const& gate_type) const {
    for (auto qubit_id : qubits) {
        if (qubit_id >= _qubits.size()) {
            spdlog::error("Qubit ID {} not found!!", qubit_id);
            return false;
        }
        
        auto* last_gate = _qubits[qubit_id].get_last_gate();
        if (last_gate != nullptr && last_gate->get_operation().get_type() == "measure") {
            spdlog::error("Cannot add gate {} to qubit {}: qubit has measurement gate as last gate", gate_type, qubit_id);
            return false;
        }
    }
    return true;
}

/**
 * @brief Validate measurement gate addition
 * 
 * @param qubit_id Qubit to measure
 * @param classical_bit_id Classical bit to store result
 * @return true if valid, false otherwise
 */
bool QCir::_validate_measurement_gate(QubitIdType qubit_id, size_t classical_bit_id) const {
    // First, validate qubit can have gate added
    if (!_validate_qubit_gate_addition({qubit_id}, "measure")) {
        return false;
    }
    
    // Check classical bit ID is valid
    if (classical_bit_id >= _classical_bits.size()) {
        spdlog::error("Classical bit ID {} not found!!", classical_bit_id);
        return false;
    }
    
    // Check if classical bit already has a value or is measured
    if (_classical_bits[classical_bit_id].has_value()) {
        spdlog::error("Cannot measure to classical bit {}: bit already has a value", classical_bit_id);
        return false;
    }
    
    if (_classical_bits[classical_bit_id].is_measured()) {
        spdlog::error("Cannot measure to classical bit {}: bit already measured", classical_bit_id);
        return false;
    }
    
    return true;
}

/**
 * @brief Validate if-else gate addition (single classical bit)
 * 
 * @param qubits Qubits the gate operates on
 * @param classical_bit_id Classical bit for condition
 * @return true if valid, false otherwise
 */
bool QCir::_validate_if_else_gate(QubitIdList const& qubits, size_t classical_bit_id) const {
    // First, validate qubits can have gates added
    if (!_validate_qubit_gate_addition(qubits, "if-else")) {
        return false;
    }
    
    // Check classical bit ID is valid
    if (classical_bit_id >= _classical_bits.size()) {
        spdlog::error("Classical bit ID {} not found!! Circuit has {} classical bits", 
                     classical_bit_id, _classical_bits.size());
        return false;
    }
    
    // Check if classical bit is determined (has value or measured)
    if (!_classical_bits[classical_bit_id].is_determined()) {
        spdlog::error("Cannot add if-else gate: classical bit {} is not determined (has_value={}, is_measured={})", 
                     classical_bit_id, _classical_bits[classical_bit_id].has_value(), _classical_bits[classical_bit_id].is_measured());
        return false;
    }
    
    return true;
}

/**
 * @brief Validate if-else gate addition (all classical bits)
 * 
 * @param qubits Qubits the gate operates on
 * @return true if valid, false otherwise
 */
bool QCir::_validate_if_else_gate_all_bits(QubitIdList const& qubits) const {
    // First, validate qubits can have gates added
    if (!_validate_qubit_gate_addition(qubits, "if-else")) {
        return false;
    }
    
    // Check if circuit has any classical bits
    if (_classical_bits.empty()) {
        spdlog::error("Cannot add if-else gate checking all bits: circuit has no classical bits");
        return false;
    }
    
    // Check if all classical bits are determined (has value or measured)
    for (size_t i = 0; i < _classical_bits.size(); ++i) {
        if (!_classical_bits[i].is_determined()) {
            spdlog::error("Cannot add if-else gate checking all bits: classical bit {} is not determined (no value or measurement)", i);
            return false;
        }
    }
    
    return true;
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

//------------------------------------------------------------------------
//   Ancilla qubit management methods
//------------------------------------------------------------------------

/**
 * @brief Add multiple ancilla qubits
 *
 * @param num number of ancilla qubits to add
 * @param state initial state of ancilla qubits (clean or dirty)
 */
void QCir::add_ancilla_qubits(size_t num, AncillaState state) {
    for (size_t i = 0; i < num; i++) {
        _qubits.emplace_back();
        _qubits.back().set_type(QubitType::ancilla);
        _qubits.back().set_ancilla_state(state);
    }
}

/**
 * @brief Add a single ancilla qubit
 *
 * @param state initial state of ancilla qubit (clean or dirty)
 */
void QCir::add_ancilla_qubit(AncillaState state) {
    add_ancilla_qubits(1, state);
}

/**
 * @brief Push a single ancilla qubit to the end
 *
 * @param state initial state of ancilla qubit (clean or dirty)
 */
void QCir::push_ancilla_qubit(AncillaState state) {
    _qubits.emplace_back();
    _qubits.back().set_type(QubitType::ancilla);
    _qubits.back().set_ancilla_state(state);
}

/**
 * @brief Insert an ancilla qubit at specific position
 *
 * @param id position to insert ancilla qubit
 * @param state initial state of ancilla qubit (clean or dirty)
 */
void QCir::insert_ancilla_qubit(QubitIdType id, AncillaState state) {
    if (id > _qubits.size()) {
        spdlog::error("Qubit ID {} is out of range!!", id);
        return;
    }
    _qubits.insert(dvlab::iterator::next(_qubits.begin(), id), QCirQubit{});
    _qubits[id].set_type(QubitType::ancilla);
    _qubits[id].set_ancilla_state(state);

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
 * @brief Remove ancilla qubit with specific id
 *
 * @param id ID of ancilla qubit to remove
 * @return true if successfully removed
 * @return false if not found, not empty, or not an ancilla qubit
 */
bool QCir::remove_ancilla_qubit(QubitIdType id) {
    if (id >= _qubits.size()) {
        spdlog::error("Qubit ID {} not found!!", id);
        return false;
    }
    if (!_qubits[id].is_ancilla()) {
        spdlog::error("Qubit ID {} is not an ancilla qubit!!", id);
        return false;
    }
    return remove_qubit(id);  // Reuse existing logic
}

/**
 * @brief Set qubit type (data or ancilla)
 *
 * @param id qubit ID
 * @param type qubit type to set
 */
void QCir::set_qubit_type(QubitIdType id, QubitType type) {
    if (id >= _qubits.size()) {
        spdlog::error("Qubit ID {} not found!!", id);
        return;
    }
    _qubits[id].set_type(type);
}

/**
 * @brief Set ancilla qubit state (clean or dirty)
 *
 * @param id qubit ID
 * @param state ancilla state to set
 */
void QCir::set_ancilla_state(QubitIdType id, AncillaState state) {
    if (id >= _qubits.size()) {
        spdlog::error("Qubit ID {} not found!!", id);
        return;
    }
    _qubits[id].set_ancilla_state(state);
}

/**
 * @brief Get qubit type
 *
 * @param id qubit ID
 * @return qubit type
 */
QubitType QCir::get_qubit_type(QubitIdType id) const {
    if (id >= _qubits.size()) {
        spdlog::error("Qubit ID {} not found!!", id);
        return QubitType::data;
    }
    return _qubits[id].get_type();
}

/**
 * @brief Get ancilla qubit state
 *
 * @param id qubit ID
 * @return ancilla state
 */
AncillaState QCir::get_ancilla_state(QubitIdType id) const {
    if (id >= _qubits.size()) {
        spdlog::error("Qubit ID {} not found!!", id);
        return AncillaState::clean;
    }
    return _qubits[id].get_ancilla_state();
}

/**
 * @brief Get list of ancilla qubit IDs
 *
 * @return vector of ancilla qubit IDs
 */
std::vector<QubitIdType> QCir::get_ancilla_qubits() const {
    std::vector<QubitIdType> ancilla_qubits;
    for (size_t i = 0; i < _qubits.size(); ++i) {
        if (_qubits[i].is_ancilla()) {
            ancilla_qubits.push_back(i);
        }
    }
    return ancilla_qubits;
}

/**
 * @brief Get list of data qubit IDs
 *
 * @return vector of data qubit IDs
 */
std::vector<QubitIdType> QCir::get_data_qubits() const {
    std::vector<QubitIdType> data_qubits;
    for (size_t i = 0; i < _qubits.size(); ++i) {
        if (_qubits[i].is_data()) {
            data_qubits.push_back(i);
        }
    }
    return data_qubits;
}

/**
 * @brief Get list of clean ancilla qubit IDs
 *
 * @return vector of clean ancilla qubit IDs
 */
std::vector<QubitIdType> QCir::get_clean_ancilla_qubits() const {
    std::vector<QubitIdType> clean_ancilla_qubits;
    for (size_t i = 0; i < _qubits.size(); ++i) {
        if (_qubits[i].is_clean_ancilla()) {
            clean_ancilla_qubits.push_back(i);
        }
    }
    return clean_ancilla_qubits;
}

/**
 * @brief Get list of dirty ancilla qubit IDs
 *
 * @return vector of dirty ancilla qubit IDs
 */
std::vector<QubitIdType> QCir::get_dirty_ancilla_qubits() const {
    std::vector<QubitIdType> dirty_ancilla_qubits;
    for (size_t i = 0; i < _qubits.size(); ++i) {
        if (_qubits[i].is_dirty_ancilla()) {
            dirty_ancilla_qubits.push_back(i);
        }
    }
    return dirty_ancilla_qubits;
}

/**
 * @brief Get number of ancilla qubits
 *
 * @return number of ancilla qubits
 */
size_t QCir::get_num_ancilla_qubits() const {
    return std::count_if(_qubits.begin(), _qubits.end(), 
                        [](const QCirQubit& q) { return q.is_ancilla(); });
}

/**
 * @brief Get number of data qubits
 *
 * @return number of data qubits
 */
size_t QCir::get_num_data_qubits() const {
    return _qubits.size() - get_num_ancilla_qubits();
}

/**
 * @brief Get number of clean ancilla qubits
 *
 * @return number of clean ancilla qubits
 */
size_t QCir::get_num_clean_ancilla_qubits() const {
    return std::count_if(_qubits.begin(), _qubits.end(), 
                        [](const QCirQubit& q) { return q.is_clean_ancilla(); });
}

/**
 * @brief Get number of dirty ancilla qubits
 *
 * @return number of dirty ancilla qubits
 */
size_t QCir::get_num_dirty_ancilla_qubits() const {
    return std::count_if(_qubits.begin(), _qubits.end(), 
                        [](const QCirQubit& q) { return q.is_dirty_ancilla(); });
}

//------------------------------------------------------------------------
//   Classical bit management methods
//------------------------------------------------------------------------

/**
 * @brief Add multiple classical bits
 *
 * @param num number of classical bits to add
 */
void QCir::add_classical_bits(size_t num) {
    for (size_t i = 0; i < num; i++) {
        _classical_bits.emplace_back();
    }
}

/**
 * @brief Add a single classical bit
 */
void QCir::add_classical_bit() {
    add_classical_bits(1);
}

/**
 * @brief Push a single classical bit to the end
 */
void QCir::push_classical_bit() {
    _classical_bits.emplace_back();
}

/**
 * @brief Insert a classical bit at specific position
 *
 * @param id position to insert classical bit
 */
void QCir::insert_classical_bit(size_t id) {
    if (id > _classical_bits.size()) {
        spdlog::error("Classical bit ID {} is out of range!!", id);
        return;
    }
    _classical_bits.insert(dvlab::iterator::next(_classical_bits.begin(), id), QCirBit{});
}

/**
 * @brief Set classical bit value
 *
 * @param id classical bit ID
 * @param value the bit value (0 or 1)
 */
void QCir::set_classical_value(size_t id, bool value) {
    if (id >= _classical_bits.size()) {
        spdlog::error("Classical bit ID {} not found!!", id);
        return;
    }
    _classical_bits[id].set_value(value);
}

/**
 * @brief Remove classical bit with specific id
 *
 * @param id ID of classical bit to remove
 * @return true if successfully removed
 * @return false if not found
 */
bool QCir::remove_classical_bit(size_t id) {
    if (id >= _classical_bits.size()) {
        spdlog::error("Classical bit ID {} not found!!", id);
        return false;
    }
    _classical_bits.erase(dvlab::iterator::next(_classical_bits.begin(), id));
    return true;
}

/**
 * @brief Set classical bit as measured
 *
 * @param id classical bit ID
 * @param measurement_gate pointer to the measurement gate
 */
void QCir::set_classical_measured(size_t id, QCirGate* measurement_gate) {
    if (id >= _classical_bits.size()) {
        spdlog::error("Classical bit ID {} not found!!", id);
        return;
    }
    _classical_bits[id].set_measured(measurement_gate);
}

/**
 * @brief Mark classical bit as measured without setting a value
 *
 * @param id classical bit ID
 * @param measurement_gate pointer to the measurement gate
 */
void QCir::mark_classical_as_measured(size_t id, QCirGate* measurement_gate) {
    if (id >= _classical_bits.size()) {
        spdlog::error("Classical bit ID {} not found!!", id);
        return;
    }
    _classical_bits[id].mark_as_measured(measurement_gate);
    // spdlog::info("Marked classical bit {} as measured by measurement gate {}", id, measurement_gate->get_id());
}

/**
 * @brief Get classical bit value
 *
 * @param id classical bit ID
 * @return the bit value (0 or 1) if determined, false otherwise
 */
bool QCir::get_classical_value(size_t id) const {
    if (id >= _classical_bits.size()) {
        spdlog::error("Classical bit ID {} not found!!", id);
        return false;
    }
    return _classical_bits[id].get_value();
}

/**
 * @brief Check if classical bit is measured
 *
 * @param id classical bit ID
 * @return true if measured, false otherwise
 */
bool QCir::is_classical_measured(size_t id) const {
    if (id >= _classical_bits.size()) {
        spdlog::error("Classical bit ID {} not found!!", id);
        return false;
    }
    return _classical_bits[id].is_measured();
}

/**
 * @brief Check if classical bit is determined
 *
 * @param id classical bit ID
 * @return true if determined (has value or measured), false otherwise
 */
bool QCir::is_classical_determined(size_t id) const {
    if (id >= _classical_bits.size()) {
        spdlog::error("Classical bit ID {} not found!!", id);
        return false;
    }
    return _classical_bits[id].is_determined();
}

/**
 * @brief Get list of classical bit IDs
 *
 * @return vector of classical bit IDs
 */
std::vector<size_t> QCir::get_classical_bit_ids() const {
    std::vector<size_t> classical_bit_ids;
    for (size_t i = 0; i < _classical_bits.size(); ++i) {
        classical_bit_ids.push_back(i);
    }
    return classical_bit_ids;
}

/**
 * @brief Get list of classical zero bits
 *
 * @return vector of classical zero bit IDs
 */
std::vector<size_t> QCir::get_classical_zero_bit_ids() const {
    std::vector<size_t> classical_zero_bit_ids;
    for (size_t i = 0; i < _classical_bits.size(); ++i) {
        if (_classical_bits[i].is_zero()) {
            classical_zero_bit_ids.push_back(i);
        }
    }
    return classical_zero_bit_ids;
}

/**
 * @brief Get list of classical one bits
 *
 * @return vector of classical one bit IDs
 */
std::vector<size_t> QCir::get_classical_one_bit_ids() const {
    std::vector<size_t> classical_one_bit_ids;
    for (size_t i = 0; i < _classical_bits.size(); ++i) {
        if (_classical_bits[i].is_one()) {
            classical_one_bit_ids.push_back(i);
        }
    }
    return classical_one_bit_ids;
}

/**
 * @brief Get list of classical unknown bits
 *
 * @return vector of classical unknown bit IDs
 */
std::vector<size_t> QCir::get_classical_unknown_bit_ids() const {
    std::vector<size_t> classical_unknown_bit_ids;
    for (size_t i = 0; i < _classical_bits.size(); ++i) {
        if (_classical_bits[i].is_unknown()) {
            classical_unknown_bit_ids.push_back(i);
        }
    }
    return classical_unknown_bit_ids;
}

/**
 * @brief Get number of classical bits
 *
 * @return number of classical bits
 */
size_t QCir::get_num_classical_bits() const {
    return _classical_bits.size();
}

/**
 * @brief Get number of classical zero bits
 *
 * @return number of classical zero bits
 */
size_t QCir::get_num_classical_zero_bits() const {
    return std::count_if(_classical_bits.begin(), _classical_bits.end(), 
                        [](const QCirBit& b) { return b.is_zero(); });
}

/**
 * @brief Get number of classical one bits
 *
 * @return number of classical one bits
 */
size_t QCir::get_num_classical_one_bits() const {
    return std::count_if(_classical_bits.begin(), _classical_bits.end(), 
                        [](const QCirBit& b) { return b.is_one(); });
}

/**
 * @brief Get number of classical unknown bits
 *
 * @return number of classical unknown bits
 */
size_t QCir::get_num_classical_unknown_bits() const {
    return std::count_if(_classical_bits.begin(), _classical_bits.end(), 
                        [](const QCirBit& b) { return b.is_unknown(); });
}

/**
 * @brief Map qubit to classical bit for measurement
 *
 * @param qubit_id qubit ID to measure
 * @param classical_bit_id classical bit ID to store measurement result
 * @return true if successfully mapped
 * @return false if qubit or classical bit not found
 */
bool QCir::measure_qubit_to_classical(QubitIdType qubit_id, size_t classical_bit_id) {
    if (qubit_id >= _qubits.size()) {
        spdlog::error("Qubit ID {} not found!!", qubit_id);
        return false;
    }
    
    if (classical_bit_id >= _classical_bits.size()) {
        spdlog::error("Classical bit ID {} not found!!", classical_bit_id);
        return false;
    }
    
    // Get the measurement gate that was just added
    auto* measurement_gate = _id_to_gates[_gate_id - 1].get();
    
    // Mark classical bit as measured (without setting a value)
    mark_classical_as_measured(classical_bit_id, measurement_gate);
    
    // Map qubit to classical bit for measurement
    // spdlog::info("Mapped qubit {} to classical bit {} for measurement", qubit_id, classical_bit_id);
    return true;
}



/**
 * @brief Get summary of qubit types in the circuit
 *
 * @return string summary of qubit distribution
 */
std::string QCir::get_qubit_type_summary() const {
    size_t data_count = get_num_data_qubits();
    size_t ancilla_count = get_num_ancilla_qubits();
    size_t classical_count = get_num_classical_bits();
    size_t clean_ancilla_count = get_num_clean_ancilla_qubits();
    size_t dirty_ancilla_count = get_num_dirty_ancilla_qubits();
    size_t classical_zero_count = get_num_classical_zero_bits();
    size_t classical_one_count = get_num_classical_one_bits();
    size_t classical_unknown_count = get_num_classical_unknown_bits();
    
    return fmt::format(
        "Circuit Summary:\n"
        "  Quantum qubits: {} ({} data, {} ancilla: {} clean, {} dirty)\n"
        "  Classical bits: {} ({} zero, {} one, {} unknown)",
        _qubits.size(), data_count, ancilla_count, clean_ancilla_count, dirty_ancilla_count,
        classical_count, classical_zero_count, classical_one_count, classical_unknown_count
    );
}

size_t QCir::append(Operation const& op, QubitIdList const& bits) {
    // Validate operation qubit count
    DVLAB_ASSERT(
        op.get_num_qubits() == bits.size(),
        fmt::format("Operation {} requires {} qubits, but {} qubits are given.", op.get_repr(), op.get_num_qubits(), bits.size()));
    
    // All gates must pass qubit validation
    if (!_validate_qubit_gate_addition(bits, op.get_type())) {
        throw std::runtime_error(fmt::format("Invalid gate addition: qubit validation failed for gate type {}", op.get_type()));
    }
    
    // Create the gate
    _id_to_gates.emplace(_gate_id, std::make_unique<QCirGate>(_gate_id, op, bits));
    _predecessors.emplace(_gate_id, std::vector<std::optional<size_t>>(bits.size(), std::nullopt));
    _successors.emplace(_gate_id, std::vector<std::optional<size_t>>(bits.size(), std::nullopt));
    assert(_id_to_gates.contains(_gate_id));
    assert(_predecessors.contains(_gate_id));
    assert(_successors.contains(_gate_id));
    auto* g = _id_to_gates[_gate_id].get();
    _gate_id++;

    // Connect to predecessor gates on each qubit
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

size_t QCir::append(Operation const& op, QubitIdType qubit_id, size_t classical_bit_id) {
    // Special handling for measurement gates
    if (op.get_type() == "measure") {
        // Validate measurement gate (includes qubit validation)
        if (!_validate_measurement_gate(qubit_id, classical_bit_id)) {
            throw std::runtime_error(fmt::format("Invalid measurement gate addition: validation failed for measuring qubit {} to classical bit {}", qubit_id, classical_bit_id));
        }
        
        // Create qubit list and classical bit list for the measurement gate
        QubitIdList bits = {qubit_id};
        ClassicalBitIdList classical_bits = {classical_bit_id};
        
        _id_to_gates.emplace(_gate_id, std::make_unique<QCirGate>(_gate_id, op, bits, classical_bits));
        _predecessors.emplace(_gate_id, std::vector<std::optional<size_t>>(bits.size(), std::nullopt));
        _successors.emplace(_gate_id, std::vector<std::optional<size_t>>(bits.size(), std::nullopt));
        assert(_id_to_gates.contains(_gate_id));
        assert(_predecessors.contains(_gate_id));
        assert(_successors.contains(_gate_id));
        auto* g = _id_to_gates[_gate_id].get();
        _gate_id++;

        // Connect the gate to the qubit
        if (_qubits[qubit_id].get_last_gate() != nullptr) {
            _connect(_qubits[qubit_id].get_last_gate()->get_id(), g->get_id(), qubit_id);
        } else {
            _qubits[qubit_id].set_first_gate(g);
        }
        _qubits[qubit_id].set_last_gate(g);
        
        // Mark classical bit as measured and link to measurement gate
        measure_qubit_to_classical(qubit_id, classical_bit_id);
        
        _dirty = true;
        return g->get_id();
    } else {
        // For non-measurement gates, use the regular append function
        QubitIdList bits = {qubit_id};
        return append(op, bits);
    }
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

size_t QCir::append(Operation const& op, QubitIdList const& bits, ClassicalBitIdType classical_bit, size_t classical_value) {
    // Create an if-else gate with single classical bit condition
    IfElseGate if_else_op(op, classical_bit, classical_value);
    
    // Validate operation qubit count
    DVLAB_ASSERT(
        if_else_op.get_num_qubits() == bits.size(),
        fmt::format("Operation {} requires {} qubits, but {} qubits are given.", if_else_op.get_repr(), if_else_op.get_num_qubits(), bits.size()));
    
    // Validate if-else gate addition (includes qubit and classical bit validation)
    if (!_validate_if_else_gate(bits, classical_bit)) {
        throw std::runtime_error(fmt::format("Invalid if-else gate addition: validation failed for classical bit {} and qubits {}", classical_bit, fmt::join(bits, ", ")));
    }
    
    // Create the gate
    _id_to_gates.emplace(_gate_id, std::make_unique<QCirGate>(_gate_id, if_else_op, bits, classical_bit, classical_value));
    _predecessors.emplace(_gate_id, std::vector<std::optional<size_t>>(bits.size(), std::nullopt));
    _successors.emplace(_gate_id, std::vector<std::optional<size_t>>(bits.size(), std::nullopt));
    assert(_id_to_gates.contains(_gate_id));
    assert(_predecessors.contains(_gate_id));
    assert(_successors.contains(_gate_id));
    auto* g = _id_to_gates[_gate_id].get();
    _gate_id++;

    // Connect to predecessor gates on each qubit
    for (auto const& qb : g->get_qubits()) {
        DVLAB_ASSERT(qb < _qubits.size(), fmt::format("Qubit {} not found!!", qb));
        if (_qubits[qb].get_last_gate() != nullptr) {
            _connect(_qubits[qb].get_last_gate()->get_id(), g->get_id(), qb);
        } else {
            _qubits[qb].set_first_gate(g);
        }
        _qubits[qb].set_last_gate(g);
    }
    
    // Connect to classical dependency: if-else gate depends on measurement gate
    if (_classical_bits[classical_bit].is_measured()) {
        auto* measurement_gate = _classical_bits[classical_bit].get_measurement_gate();
        if (measurement_gate != nullptr && !measurement_gate->get_qubits().empty()) {
            QubitIdType measured_qubit = measurement_gate->get_qubits()[0];
            QubitIdType if_else_qubit = g->get_qubits()[0];
                _connect_classical(measurement_gate->get_id(), g->get_id(), measured_qubit, if_else_qubit);
        }
    }
    
    _dirty = true;
    return g->get_id();
}

size_t QCir::append(Operation const& op, QubitIdList const& bits, size_t classical_value) {
    // Create an if-else gate checking all classical bits
    IfElseGate if_else_op(op, classical_value);
    
    // Validate operation qubit count
    DVLAB_ASSERT(
        if_else_op.get_num_qubits() == bits.size(),
        fmt::format("Operation {} requires {} qubits, but {} qubits are given.", if_else_op.get_repr(), if_else_op.get_num_qubits(), bits.size()));
    
    // Validate if-else gate addition (includes qubit and all classical bits validation)
    if (!_validate_if_else_gate_all_bits(bits)) {
        throw std::runtime_error(fmt::format("Invalid if-else gate addition: validation failed for checking all classical bits with qubits {}", fmt::join(bits, ", ")));
    }
    
    // Create the gate
    _id_to_gates.emplace(_gate_id, std::make_unique<QCirGate>(_gate_id, if_else_op, bits, classical_value));
    _predecessors.emplace(_gate_id, std::vector<std::optional<size_t>>(bits.size(), std::nullopt));
    _successors.emplace(_gate_id, std::vector<std::optional<size_t>>(bits.size(), std::nullopt));
    assert(_id_to_gates.contains(_gate_id));
    assert(_predecessors.contains(_gate_id));
    assert(_successors.contains(_gate_id));
    auto* g = _id_to_gates[_gate_id].get();
    _gate_id++;

    // Connect to predecessor gates on each qubit
    for (auto const& qb : g->get_qubits()) {
        DVLAB_ASSERT(qb < _qubits.size(), fmt::format("Qubit {} not found!!", qb));
        if (_qubits[qb].get_last_gate() != nullptr) {
            _connect(_qubits[qb].get_last_gate()->get_id(), g->get_id(), qb);
        } else {
            _qubits[qb].set_first_gate(g);
        }
        _qubits[qb].set_last_gate(g);
    }
    
    // Connect to classical dependencies: if-else gate depends on all measurement gates
    for (size_t i = 0; i < _classical_bits.size(); ++i) {
        if (_classical_bits[i].is_measured()) {
            auto* measurement_gate = _classical_bits[i].get_measurement_gate();
            if (measurement_gate != nullptr && !measurement_gate->get_qubits().empty()) {
                QubitIdType measured_qubit = measurement_gate->get_qubits()[0];
                QubitIdType if_else_qubit = g->get_qubits()[0];
                    _connect_classical(measurement_gate->get_id(), g->get_id(), measured_qubit, if_else_qubit);
            }
        }
    }
    
    _dirty = true;
    return g->get_id();
}

size_t QCir::append(QCirGate const& gate) {
    auto const& op = gate.get_operation();
    auto const qubits = gate.get_qubits();
    
    // Handle gates with classical bits
    if (gate.has_classical_bits()) {
        auto const& classical_bits = gate.get_classical_bits();
        auto const classical_value = gate.get_classical_value();
        
        // Check if this is a measurement gate (has classical bits but no classical value)
        if (op.get_type() == "measure" && classical_bits.size() == 1 && !classical_value.has_value()) {
            DVLAB_ASSERT(qubits.size() == 1, "Measurement gate must have exactly one qubit");
            return append(op, qubits[0], classical_bits[0]);
        }
        
        // Check if this is an if-else gate (has classical value)
        if (auto if_else_op = op.get_underlying_if<IfElseGate>(); if_else_op) {
            // Extract the underlying operation from the IfElseGate
            auto const& underlying_op = if_else_op->get_operation();
            
            // If-else gate with all classical bits
            if (classical_bits.empty() && classical_value.has_value()) {
                return append(underlying_op, qubits, *classical_value);
            }
            // If-else gate with single classical bit
            else if (classical_bits.size() == 1 && classical_value.has_value()) {
                return append(underlying_op, qubits, classical_bits[0], *classical_value);
            }
        }
        
        // If we have classical bits but didn't match above cases, log warning
        spdlog::warn("Gate with classical bits not handled properly: type={}, classical_bits.size()={}, has_value={}", 
                    op.get_type(), classical_bits.size(), classical_value.has_value());
    }
    
    // Regular gate without classical bits
    return append(op, qubits);
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
