/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define tensor decomposer structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./decomposer.hpp"

namespace qsyn::tensor {

/**
 * @brief Append gate and save the encoding gate sequence
 *
 * @param target
 * @param qubit_list
 * @param gate_list
 */
void Decomposer::_encode_control_gate(QubitIdList const& target, std::vector<QubitIdList>& qubit_list, std::vector<std::string>& gate_list) {
    qubit_list.emplace_back(target);
    gate_list.emplace_back(target.size() == 2 ? "cx" : "x");
    _quantum_circuit.add_gate(target.size() == 2 ? "cx" : "x", target, {}, true);
}

/**
 * @brief Gray encoder
 *
 * @param origin_pos Origin qubit
 * @param targ_pos Target qubit
 * @param qubit_list
 * @param gate_list
 */
void Decomposer::_encode(size_t origin_pos, size_t targ_pos, std::vector<QubitIdList>& qubit_list, std::vector<std::string>& gate_list) {
    bool x_given = false;
    if (((origin_pos >> targ_pos) & 1) == 0) {
        _encode_control_gate({int(targ_pos)}, qubit_list, gate_list);
        x_given = true;
    }
    for (size_t i = 0; i < _n_qubits; i++) {
        if (i == targ_pos) continue;
        if (((origin_pos >> i) & 1) == 0)
            _encode_control_gate({int(targ_pos), int(i)}, qubit_list, gate_list);
    }
    if (x_given)
        _encode_control_gate({int(targ_pos)}, qubit_list, gate_list);
}

}  // namespace qsyn::tensor
