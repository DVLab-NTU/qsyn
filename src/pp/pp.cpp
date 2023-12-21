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



namespace qsyn::pp{


/**
 * @brief Calculate phase polynomial of the circuit
 * @param Qcir*
 * @return Polynomial
 */
bool Phase_Polynomial::calculate_pp(QCir const& qc){
    
    std::cout<<"In function calculate_pp\n" << endl;

    _qubit_number = qc.get_num_qubits();

    Phase_Polynomial::reset();

    std::vector<QCirGate*> gates = qc.get_topologically_ordered_gates();

    for(QCirGate* g: gates){
        if (g->is_cx()){
            std::cout<<"Find a cx" << endl;
            _wires.row_operation(g->get_control()._qubit, g->get_targets()._qubit);
        }
        else if (g->get_num_qubits() == 1 && 
                (g->get_rotation_category() == GateRotationCategory::pz || 
                g->get_rotation_category() == GateRotationCategory::rz)){
            std::cout<<"Find a single zphase " << g->get_phase().get_print_string() << endl;
            Phase_Polynomial::insert_phase(g->get_control()._qubit, g->get_phase());
        }
        else{
            std::cout<<"Find a unsupport gate " << g->get_type_str() << endl;
        }
    }
    return true;

}

/**
 * @brief Add phase into polynomial
 * @param size_t q: qubit
 * @param Phase phase: z rotation phase
 * @return true is add successful
 */
bool Phase_Polynomial::insert_phase(size_t q, dvlab::Phase phase){
    
    std::cout<< "In function insert_phase, q: " << q << " phase: "<< phase.get_print_string() <<endl;

    dvlab::BooleanMatrix::Row term(_wires.get_row(q));
    if(_pp_terms.find_row(term).has_value()){
        size_t q = (_pp_terms.find_row(term).value());
        std::cout << "Row exist at " << q << endl;
        _pp_terms.print_matrix();
        std::cout << "Before phase:  " << _pp_coeff[q].get_print_string() << endl;
        _pp_coeff[q] += phase;
        std::cout << "After phase:  " << _pp_coeff[q].get_print_string() << endl;
    }else{
        std::cout << "New row" << endl;
        _pp_terms.push_row(term);
        _pp_coeff.push_back(phase);
    }

    return true;
}



/**
 * @brief Reset the phase poly and wires
 * @param 
 *
 */
void Phase_Polynomial::reset(){
    _pp_terms.clear();
    _pp_coeff.clear();
    Phase_Polynomial::intial_wire(_qubit_number);
}


/**
 * @brief Initial wire into q_0, q_1, q_2 ......
 * @param size_t n: qubit number
 *
 */
void Phase_Polynomial::intial_wire(size_t n){
    _wires.clear();
    dvlab::BooleanMatrix temp(n,n);
    for (size_t i = 0; i < n; i++) temp[i][i] = 1;
    _wires = temp;
    // _wires.print_matrix();
}



}