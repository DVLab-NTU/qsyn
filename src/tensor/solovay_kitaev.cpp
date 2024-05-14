/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define tensor Solovay-Kitaev algorithm structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./solovay_kitaev.hpp"

#include <sul/dynamic_bitset.hpp>

#include "qcir/basic_gate_type.hpp"

namespace qsyn::tensor {

/**
 * @brief Initialize binary list
 *
 */
void SolovayKitaev::_init_binary_list() {
    for (size_t i = 1; i <= _depth; i++)
        for (size_t j = 0; j < gsl::narrow<size_t>(std::pow(2, i)); j++)
            _binary_list.emplace_back(sul::dynamic_bitset<>(i, j));
}

/**
 * @brief Adjoint the gate sequence
 *
 * @param sequence
 * @return std::vector<int>
 */
std::vector<int> SolovayKitaev::_adjoint_gate_sequence(std::vector<int> sequence) const {
    std::reverse(sequence.begin(), sequence.end());
    std::transform(sequence.cbegin(), sequence.cend(), sequence.begin(), std::negate<int>());
    return sequence;
}

/**
 * @brief Remove redundant gates
 *
 * @param gate_sequence
 */
void SolovayKitaev::_remove_redundant_gates(std::vector<int>& gate_sequence) const {
    const size_t original_count = gate_sequence.size();
    size_t counter_rotate       = 0;
    std::vector<int> optimized_sequence;
    while (true) {
        for (const auto& bit : gate_sequence) {
            if (bit != 0) {  // Rotate
                counter_rotate += bit;
            } else {  // H
                if (counter_rotate != 0) {
                    optimized_sequence.emplace_back(counter_rotate);
                    counter_rotate = 0;
                }
                optimized_sequence.emplace_back(0);
            }
        }
        if (counter_rotate != 0) {
            optimized_sequence.emplace_back(counter_rotate);
            counter_rotate = 0;
        }
        for (auto it = optimized_sequence.begin(); it < optimized_sequence.end() - 1; it++) {
            if (*it == *(it + 1)) {
                optimized_sequence.erase(it);
                optimized_sequence.erase(it);
                it--;
            }
        }
        if (optimized_sequence.size() == gate_sequence.size())
            break;
        else
            spdlog::trace("Remove {} gates", gate_sequence.size() - optimized_sequence.size());
        gate_sequence = optimized_sequence;
        optimized_sequence.clear();
    }
    spdlog::info("Remove {} redundant gates", original_count - gate_sequence.size());
}

/**
 * @brief Save gates in sequence into QCir
 *
 * @param gate_sequence
 */
void SolovayKitaev::_save_gates(const std::vector<int>& gate_sequence) {
    _quantum_circuit.add_qubits(1);
    for (const int& bit : gate_sequence) {
        if (bit == 0) {
            _quantum_circuit.prepend(qcir::HGate(), {0});
        } else {
            _quantum_circuit.prepend(qcir::PZGate(Phase(bit, 4)), {0});
        }
    }
    spdlog::info("Decompose tensor into {} gates.", _quantum_circuit.get_num_gates());
}

}  // namespace qsyn::tensor
