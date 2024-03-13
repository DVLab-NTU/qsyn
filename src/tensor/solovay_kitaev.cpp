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
 * @brief Dagger the sequence
 *
 * @param sequence
 */
void SolovayKitaev::_dagger_matrices(std::vector<int>& sequence) {
    std::reverse(sequence.begin(), sequence.end());
    std::transform(sequence.cbegin(), sequence.cend(), sequence.begin(), std::negate<int>());
}

// /**
//  * @brief Remove redundant gates, e.g. HH = I and TTTTTTTT = I (Outdated)
//  *
//  * @param gate_sequence
//  */

void SolovayKitaev::_remove_redundant_gates(std::vector<int>& gate_sequence) {
    std::vector<int> result;
    size_t counter = 0;
     while (counter < gate_sequence.size() - 1) {
        // T T T T T T T T         
        if (gate_sequence[counter] == 1 && gate_sequence[counter + 1] == 1 && 
                gate_sequence[counter + 2] == 1 && gate_sequence[counter + 3] == 1 && 
                gate_sequence[counter + 4] == 1 && gate_sequence[counter + 5] == 1 && 
                gate_sequence[counter + 6] == 1 && gate_sequence[counter + 7] == 1 && counter + 7 < gate_sequence.size()) {
            for(int i = 0; i < 8; i++){
                gate_sequence.erase(gate_sequence.begin() + counter);
            }
            if(counter > 0)
            {
                counter--;
            }
            
        // Td Td Td Td Td Td Td Td
        else if (gate_sequence[counter] == -1 && gate_sequence[counter + 1] == -1 && 
                gate_sequence[counter + 2] == -1 && gate_sequence[counter + 3] == -1 && 
                gate_sequence[counter + 4] == -1 && gate_sequence[counter + 5] == -1 && 
                gate_sequence[counter + 6] == -1 && gate_sequence[counter + 7] == -1 && counter + 7 < gate_sequence.size()) {
            for(int i = 0; i < 8; i++){
                gate_sequence.erase(gate_sequence.begin() + counter);
            }
            
            if(counter > 0)
            {
                counter--;
            }
        } 
        // TTTT = Z
        else if (gate_sequence[counter] == 1 && gate_sequence[counter + 1] == 1 && 
                 gate_sequence[counter + 2] == 1 && gate_sequence[counter + 3] == 1 
                 && counter + 3 < gate_sequence.size()) {
            for(int i = 0; i < 3; i++){
                gate_sequence.erase(gate_sequence.begin() + counter);
            }
            gate_sequence[counter] = 4;
            if(counter > 0)
            {
                counter--;
            }
            
        }
        // HSSH = HZH = X
        else if (gate_sequence[counter] == 0 && gate_sequence[counter + 1] == 2 && 
                gate_sequence[counter + 2] == 2 && gate_sequence[counter + 3] == 0 
                && counter + 3 < gate_sequence.size()) {
            for(int i = 0; i < 3; i++){
                gate_sequence.erase(gate_sequence.begin() + counter);
            }
            gate_sequence[counter] = -4;
            if(counter > 0)
            {
                counter--;
            }
            
        }
        // T Td
        if (gate_sequence[counter] == 1 && gate_sequence[counter + 1] == -1 && counter + 1 < gate_sequence.size()) {
            gate_sequence.erase(gate_sequence.begin() + counter);
            gate_sequence.erase(gate_sequence.begin() + counter);
            if(counter > 0)
            {
                counter--;
            }
            
        } 
        // Td T
        else if (gate_sequence[counter] == -1 && gate_sequence[counter + 1] == 1 && counter + 1 < gate_sequence.size()) {
            gate_sequence.erase(gate_sequence.begin() + counter);
            gate_sequence.erase(gate_sequence.begin() + counter);
            if(counter > 0)
            {
                counter--;
            }
            
        } 
        // H H
        else if (gate_sequence[counter] == 0 && gate_sequence[counter + 1] == 0 && counter + 1 < gate_sequence.size()) {
            gate_sequence.erase(gate_sequence.begin() + counter);
            gate_sequence.erase(gate_sequence.begin() + counter);
            if(counter > 0)
            {
                counter--;
            }
            
        }
        // S Sd
        else if (gate_sequence[counter] == 2 && gate_sequence[counter + 1] == -2 && counter + 1 < gate_sequence.size()) {
            gate_sequence.erase(gate_sequence.begin() + counter);
            gate_sequence.erase(gate_sequence.begin() + counter);
            if(counter > 0)
            {
                counter--;
            }
            
        }
        // Sd S 
        else if (gate_sequence[counter] == -2 && gate_sequence[counter + 1] == 2 && counter + 1 < gate_sequence.size()) {
            gate_sequence.erase(gate_sequence.begin() + counter);
            gate_sequence.erase(gate_sequence.begin() + counter);
            if(counter > 0)
            {
                counter--;
            }
            
        }
        // Td Td = Sd
        else if (gate_sequence[counter] == -1 && gate_sequence[counter + 1] == -1 && counter + 1 < gate_sequence.size()) {
            gate_sequence.erase(gate_sequence.begin() + counter);
            gate_sequence[counter] = -2;
            if(counter > 0)
            {
                counter--;
            }
            
        } 
        // T T = S
        else if (gate_sequence[counter] == 1 && gate_sequence[counter + 1] == 1 && counter + 1 < gate_sequence.size()) {
            gate_sequence.erase(gate_sequence.begin() + counter);
            gate_sequence[counter] = 2;
            if(counter > 0)
            {
                counter--;
            }
            
        }   
        else
        {
            counter++;
        }
    }
}

/**
 * @brief Save gates in sequence into QCir
 *
 * @param gate_sequence
 */
void SolovayKitaev::_save_gates(const std::vector<int>& gate_sequence) {
    
    _quantum_circuit.add_qubits(1);
    for (const int& bit : gate_sequence)
        _quantum_circuit.add_gate((bit == 1 ? "T" : bit == 0 ? "H"
                                : bit == -1 ? "TDG" : bit == 2 ? "S"
                                : bit == -2 ? "SDG" : bit == 4 ? "Z"
                                :  "X"),
                                  {0}, {}, false);
    spdlog::info("Decompose tensor into {} gates.", _quantum_circuit.get_num_gates());
}

}  // namespace qsyn::tensor
