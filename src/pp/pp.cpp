/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Define class Phase_Polynomial structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./pp.hpp"
#include <spdlog/common.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <iomanip>   // std::setw

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
 
    Phase_Polynomial::count_t_depth(qc);

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
            dvlab::BooleanMatrix prev_wires = _wires;
            
            // std::cout << "Before H " << endl;
            // _wires.print_matrix(spdlog::level::level_enum::off);
            _wires[q] = h_output_state;
            // std::cout << "After H " << endl;
            // _wires.print_matrix(spdlog::level::level_enum::off);
            _h_map.emplace_back(std::make_pair(prev_wires, _wires));
            _h.emplace_back(q);
        } else {
            std::cout << "Find a unsupport gate " << g->get_type_str() << endl;
            return false;
        }
    }
    Phase_Polynomial::remove_coeff_0_monomial();
    Phase_Polynomial::extend_h_map();
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
 * @brief Extend H map to right size
 * @param
 *
 */
void Phase_Polynomial::extend_h_map(){
    size_t total_variable = _wires.num_cols();
    for(auto& [first, second]: _h_map){
        while(first.num_cols()<total_variable) first.push_zeros_column();
        while(second.num_cols()<total_variable) second.push_zeros_column(); ;
        assert(first.num_cols() == _wires.num_cols());
        assert(second.num_cols() == _wires.num_cols());
    };
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
    QCir circuit;
    circuit.add_qubits(_qubit_number);
    _result = circuit;
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

/**
 * @brief Print the info of phase polynomial
 *
 */
void Phase_Polynomial::print_h_map(spdlog::level::level_enum lvl) const {
    spdlog::log(lvl, "H map");
    for(auto const &[first, second]: _h_map){
        spdlog::log(lvl, "Before: ");
        first.print_matrix();
        spdlog::log(lvl, "After: ");
        second.print_matrix();
    }
}

/**
 * @brief Print  phase  and polynomial in the same line
 *
 */
void Phase_Polynomial::print_phase_poly(spdlog::level::level_enum lvl) const {
    spdlog::log(lvl, "\n  Phase Polynomial");
    for(size_t i=0; i<_pp_terms.num_rows(); i++){
        
        cout << "Phase: "<< _pp_coeff[i].get_print_string() << endl;
        cout << "Term : ";
        _pp_terms[i].print_row(lvl);
        cout << endl;
    }
    }

/**
 * @brief Count t-depth
 * @return size_t: t-depth
 */
size_t Phase_Polynomial::count_t_depth(qcir::QCir const& qcir) {
    vector<size_t> depths(qcir.get_num_qubits());
    std::vector<QCirGate*> gates = qcir.get_topologically_ordered_gates();
    for (QCirGate* g : gates) {
        if (g->is_cx()) {
            size_t ctrl = g->get_control()._qubit, targ = g->get_targets()._qubit;
            if(depths[ctrl] < depths[targ]) depths[ctrl] = depths[targ];
            else depths[targ] = depths[ctrl];
        } else if (g->get_num_qubits() == 1 &&
                   (g->get_rotation_category() == GateRotationCategory::pz ||
                    g->get_rotation_category() == GateRotationCategory::rz)) {
            if(g->get_phase().denominator() == 4) depths[g->get_control()._qubit]++;
        }   
    }
    auto it = max_element(depths.begin(), depths.end());
    spdlog::log(spdlog::level::level_enum::off, "T depth of the circuit is {}", *it);
    return *it;
}


}  // namespace qsyn::pp
