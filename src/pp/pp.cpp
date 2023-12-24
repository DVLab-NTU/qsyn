/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Define class Phase_Polynomial structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./pp.hpp"

#include <iostream>

#include "qcir/qcir.hpp"
#include "qcir/qcir_gate.hpp"
#include "qcir/qcir_qubit.hpp"
#include "qsyn/qsyn_type.hpp"
#include "util/boolean_matrix.hpp"

using namespace qsyn::qcir;
using namespace std;

namespace qsyn::pp {

using Row = dvlab::BooleanMatrix::Row;

/**
 * @brief Calculate phase polynomial of the circuit
 * @param Qcir*
 * @return Polynomial
 */
bool Phase_Polynomial::calculate_pp(QCir const& qc) {
    _qubit_number = qc.get_num_qubits();

    Phase_Polynomial::reset();

    std::vector<QCirGate*> gates = qc.get_topologically_ordered_gates();

    for (QCirGate* g : gates) {
        if (g->is_cx()) {
            _wires.row_operation(g->get_control()._qubit, g->get_targets()._qubit);
        } else if (g->get_num_qubits() == 1 &&
                   (g->get_rotation_category() == GateRotationCategory::pz ||
                    g->get_rotation_category() == GateRotationCategory::rz)) {
            Phase_Polynomial::insert_phase(g->get_control()._qubit, g->get_phase());
        } else if (g->is_h()) {
            size_t q = g->get_control()._qubit;
            _hadamard.push_back(g);
            // todo: check if the rank need to be increased

            // if rank need to be increased
            _pp_terms.push_zeros_column();
            _wires.push_zeros_column();
            Row h_output_state(_wires.num_cols());
            h_output_state[_wires.num_cols() - 1] = 1;
            h_output_state.print_row();
            dvlab::BooleanMatrix prev_wires = _wires;
            
            std::cout << "Before H " << endl;
            _wires.print_matrix(spdlog::level::level_enum::off);
            _wires[q] = h_output_state;
            std::cout << "After H " << endl;
            _wires.print_matrix(spdlog::level::level_enum::off);

            // Todo: extend h map
            _h_map.emplace_back(std::make_pair(prev_wires, _wires));
        } else {
            std::cout << "Find a unsupport gate " << g->get_type_str() << endl;
            return false;
        }
    }
    Phase_Polynomial::remove_coeff_0_monomial();
    return true;
}

/**
 * @brief Add phase into polynomial
 * @param size_t q: qubit
 * @param Phase phase: z rotation phase
 * @return true is add successful
 */
bool Phase_Polynomial::insert_phase(size_t q, dvlab::Phase phase) {
    dvlab::BooleanMatrix::Row term(_wires.get_row(q));
    if (_pp_terms.find_row(term).has_value()) {
        size_t q = (_pp_terms.find_row(term).value());
        _pp_coeff[q] += phase;
    } else {
        _pp_terms.push_row(term);
        _pp_coeff.push_back(phase);
    }

    return true;
}

/**
 * @brief Remove the monomial that coeff = 0
 * @param
 *
 */
void Phase_Polynomial::remove_coeff_0_monomial() {
    vector<size_t> coeff_is_0;
    for (size_t i = 0; i < _pp_coeff.size(); i++) {
        if (_pp_coeff[i] == dvlab::Phase(0)) coeff_is_0.emplace_back(i);
    }
    for_each(coeff_is_0.begin(), coeff_is_0.end(), [](size_t i) { std::cout << i << endl; });
    for (int i = coeff_is_0.size() - 1; i >= 0; i--) {
        _pp_terms.erase_row(coeff_is_0[i]);
        _pp_coeff.erase(_pp_coeff.begin() + coeff_is_0[i]);
    }
}

/**
 * @brief Reset the phase poly and wires
 * @param
 *
 */
void Phase_Polynomial::reset() {
    _pp_terms.clear();
    _pp_coeff.clear();
    Phase_Polynomial::intial_wire(_qubit_number);
}

/**
 * @brief Initial wire into q_0, q_1, q_2 ......
 * @param size_t n: qubit number
 *
 */
void Phase_Polynomial::intial_wire(size_t n) {
    _wires.clear();
    dvlab::BooleanMatrix temp(n, n);
    for (size_t i = 0; i < n; i++) temp[i][i] = 1;
    _wires = temp;
    // _wires.print_matrix();
}

/**
 * @brief Print the info of wires
 *
 */
void Phase_Polynomial::print_wires(spdlog::level::level_enum lvl) const {
    spdlog::log(lvl, "Polynomial wires");
    _wires.print_matrix(lvl);
}

/**
 * @brief Print the info of phase polynomial
 *
 */
void Phase_Polynomial::print_polynomial(spdlog::level::level_enum lvl) const {
    spdlog::log(lvl, "Polynomial terms");
    _pp_terms.print_matrix(lvl);
    spdlog::log(lvl, "Polynomial coefficient");
    for_each(_pp_coeff.begin(), _pp_coeff.end(), [&](dvlab::Phase p) { spdlog::log(lvl, p.get_print_string()); });
}

}  // namespace qsyn::pp
