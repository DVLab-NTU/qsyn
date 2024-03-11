/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define tensor Solovay-Kitaev algorithm structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./solovay_kitaev.hpp"

#include <sul/dynamic_bitset.hpp>

namespace qsyn::tensor {

/**
 * @brief Initialize binary list
 *
 * @return BinaryList
 */
BinaryList SolovayKitaev::_init_binary_list() const {
    BinaryList bin_list;
    for (size_t i = 1; i <= _depth; i++) {
        for (size_t j = 0; j < gsl::narrow<size_t>(std::pow(2, i)); j++) {
            sul::dynamic_bitset<> bitset(i, j);
            std::vector<bool> bit_vector;
            for (size_t k = 0; k < i; ++k)
                bit_vector.emplace_back(bitset[k]);
            bin_list.emplace_back(bit_vector);
        }
    }
    return bin_list;
}

/**
 * @brief Remove redundant gates, e.g. HH = I and TTTTTTTT = I
 *
 * @param gate_sequence
 */
void SolovayKitaev::_remove_redundant_gates(std::vector<bool>& gate_sequence) {
    size_t counter = 0;
    while (counter < gate_sequence.size() - 1) {
        if (!gate_sequence[counter] && !gate_sequence[counter + 1]) {
            gate_sequence.erase(gate_sequence.begin() + gsl::narrow<int>(counter), gate_sequence.begin() + gsl::narrow<int>(counter) + 2);
            if (counter < 8)
                counter = 0;
            else
                counter -= 8;
        } else if (gate_sequence[counter] && gate_sequence[counter + 1] && gate_sequence[counter + 2] && gate_sequence[counter + 3] && gate_sequence[counter + 4] && gate_sequence[counter + 5] && gate_sequence[counter + 6] && gate_sequence[counter + 7]) {
            gate_sequence.erase(gate_sequence.begin() + gsl::narrow<int>(counter), gate_sequence.begin() + gsl::narrow<int>(counter) + 8);
        } else
            counter++;
    }
}

void SolovayKitaev::_save_gates(const std::vector<bool>& gate_sequence) {
    fmt::println("Gate sequence");
    for (const auto& a : gate_sequence) {
        fmt::print("{} ", size_t(a));
    }
    fmt::println("");
    _quantum_circuit.add_qubits(1);
    for (const bool& bit : gate_sequence)
        _quantum_circuit.add_gate((bit ? "T" : "H"), {0}, {}, true);
    _quantum_circuit.print_circuit_diagram();
    spdlog::info("Decompose tensor into {} gates.", _quantum_circuit.get_num_gates());
}



}  // namespace qsyn::tensor
