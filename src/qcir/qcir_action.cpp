/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCir Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/format.h>

#include <cassert>
#include <queue>
#include <stack>
#include <unordered_set>

#include "qcir/operation.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "qcir/qcir_qubit.hpp"
#include "tl/to.hpp"

namespace qsyn::qcir {

/**
 * @brief Append the target to current QCir
 *
 * @param other
 * @return QCir*
 */
QCir& QCir::compose(QCir const& other) {
    // Ensure enough qubits
    if (get_num_qubits() < other.get_num_qubits()) {
        add_qubits(other.get_num_qubits() - get_num_qubits());
    }
    
    // Ensure enough classical bits
    if (get_num_classical_bits() < other.get_num_classical_bits()) {
        add_classical_bits(other.get_num_classical_bits() - get_num_classical_bits());
    }
    
    // Copy classical bit states from other circuit
    for (size_t i = 0; i < other.get_num_classical_bits(); ++i) {
        auto const& other_bit = other.get_classical_bits()[i];
        if (other_bit.has_value() && !_classical_bits[i].has_value()) {
            _classical_bits[i].set_value(other_bit.get_value());
        }
        // Note: Don't copy measurement state - gates will establish that
    }
    
    // Append gates from other circuit
    for (auto& targ_gate : other.get_gates()) {
        append(*targ_gate);
    }
    return *this;
}

/**
 * @brief Tensor the target to current tensor of QCir
 *
 * @param other
 * @return QCir*
 */
QCir& QCir::tensor_product(QCir const& other) {
    size_t const old_num_qubits = get_num_qubits();
    add_qubits(other.get_num_qubits());
    for (auto& targ_gate : other.get_gates()) {
        auto const old_qubits = targ_gate->get_qubits();
        auto const qubits     = old_qubits | std::views::transform([&](auto qubit) {
                                return qubit + old_num_qubits;
                            }) |
                            tl::to<QubitIdList>();
        append(targ_gate->get_operation(), qubits);
    }
    return *this;
}

namespace {

/**
 * @brief Perform DFS from currentGate
 *
 * @param currentGate the gate to start DFS
 */
std::vector<QCirGate*> dfs(QCir const& qcir) {
    // Use Kahn's algorithm for topological sort to properly handle all dependencies
    std::vector<QCirGate*> topo_order;
    std::unordered_map<size_t, size_t> in_degree;  // gate_id -> number of unvisited predecessors
    std::queue<QCirGate*> ready_queue;
    std::unordered_set<size_t> seen_gates;
    
    // First pass: collect all gates by traversing from last gates on each qubit backwards
    // This ensures we find all gates including those only connected via classical dependencies
    std::stack<QCirGate*> discovery_stack;
    for (auto const& qubit : qcir.get_qubits()) {
        if (qubit.get_last_gate() != nullptr) {
            discovery_stack.push(qubit.get_last_gate());
        }
    }

    while (!discovery_stack.empty()) {
        auto* gate = discovery_stack.top();
        discovery_stack.pop();
        
        if (seen_gates.contains(gate->get_id())) {
            continue;
        }
        seen_gates.insert(gate->get_id());

        // Calculate in-degree for this gate (count unique predecessors)
        auto const& predecessors = qcir.get_predecessors(gate->get_id());
        std::unordered_set<size_t> unique_preds;
        for (auto const& pred : predecessors) {
            if (pred.has_value()) {
                unique_preds.insert(*pred);
            }
        }
        in_degree[gate->get_id()] = unique_preds.size();
        
        // Gates with no predecessors are ready to process
        if (unique_preds.size() == 0) {
            ready_queue.push(gate);
        }
        
        // Discover predecessors (traverse backwards)
        for (auto const& pred_id : unique_preds) {
            if (!seen_gates.contains(pred_id)) {
                discovery_stack.push(qcir.get_gate(pred_id));
            }
        }
    }
    
    // Process gates in topological order using Kahn's algorithm
    while (!ready_queue.empty()) {
        auto* gate = ready_queue.front();
        ready_queue.pop();
        topo_order.push_back(gate);
        
        // For each successor, decrement its in-degree
        auto const& successors = qcir.get_successors(gate->get_id());
        std::unordered_set<size_t> unique_succs;
        for (auto const& succ_id : successors) {
            if (succ_id.has_value()) {
                unique_succs.insert(*succ_id);
            }
        }
        
        for (auto const& succ_id : unique_succs) {
            if (in_degree.contains(succ_id)) {
                in_degree[succ_id]--;
                if (in_degree[succ_id] == 0) {
                    ready_queue.push(qcir.get_gate(succ_id));
            }
        }
    }
    }
    
    // Check if all gates were processed (no cycles)
    if (topo_order.size() != seen_gates.size()) {
        spdlog::error("Topological sort failed: processed {} gates but found {} gates total", 
                     topo_order.size(), seen_gates.size());
    }

    return topo_order;
}

}  // namespace

/**
 * @brief Update topological order
 *
 * @return const vector<QCirGate*>&
 */
void QCir::_update_topological_order() const {
    if (!_dirty)
        return;

    _gate_list = dfs(*this);
    assert(_gate_list.size() == get_num_gates());

    _dirty = false;
}

/**
 * @brief Reset QCir
 *
 */
void QCir::reset() {
    _qubits.clear();
    _classical_bits.clear();
    _gate_list.clear();
    _id_to_gates.clear();
    _predecessors.clear();
    _successors.clear();

    _gate_id = 0;
    _dirty   = true;
}

void QCir::adjoint_inplace() {
    for (auto& g : _id_to_gates | std::views::values) {
        g->set_operation(qsyn::qcir::adjoint(g->get_operation()));
        auto old_succs = get_successors(g->get_id());
        auto old_preds = get_predecessors(g->get_id());
        _set_successors(g->get_id(), old_preds);
        _set_predecessors(g->get_id(), old_succs);
    }

    for (auto& q : _qubits) {
        auto first = q.get_first_gate();
        auto last  = q.get_last_gate();
        q.set_first_gate(last);
        q.set_last_gate(first);
    }

    _dirty = true;
}

void QCir::concat(
    QCir const& other,
    std::map<QubitIdType /* new */, QubitIdType /* orig */> const& qubit_map) {
    for (auto const& new_gate : other.get_gates()) {
        auto const tmp =
            new_gate->get_qubits();  // circumvent g++ 11.4 compilation bug
        auto const qubits = tmp | std::views::transform([&qubit_map](auto const& qubit) {
                                return qubit_map.at(qubit);
                            }) |
                            tl::to<std::vector>();
        append(new_gate->get_operation(), qubits);
    }
}

}  // namespace qsyn::qcir
