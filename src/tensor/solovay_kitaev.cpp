/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define tensor Solovay-Kitaev algorithm structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./solovay_kitaev.hpp"

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
            boost::dynamic_bitset<> bitset(i, j);
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

}  // namespace qsyn::tensor
