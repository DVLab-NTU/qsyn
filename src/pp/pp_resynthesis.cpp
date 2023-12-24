/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Implement resynthesis algorithm ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "pp.hpp"
#include "pp_partition.hpp"

#include "util/boolean_matrix.hpp"
#include "qcir/qcir.hpp"

#include <iostream>


using namespace qsyn::qcir;
using namespace std;

namespace qsyn::pp {
/**
 * @brief Preform gaussian to resynthesis
 * @param Partitions p
 * @param Wires initial_wires
 * @para Wires terminal_wires
 * @return QCir
 */
void Phase_Polynomial::gaussian_resynthesis(Partitions partitions, Wires initial_wires, Wires terminal_wires) {
    cout << "Into resynthesis" << endl;
    for_each(partitions.begin(), partitions.end(), [&](dvlab::BooleanMatrix m){cout<<"partition"<<endl;m.print_matrix();});


    initial_wires.gaussian_elimination(true, false);
    auto cnots_initial = initial_wires.get_row_operations();
    reverse(cnots_initial.begin(), cnots_initial.end());
    for(auto [ctrl, targ]: cnots_initial){
        QubitIdList qubit_list;
        qubit_list.emplace_back(std::move(ctrl));
        qubit_list.emplace_back(std::move(targ));
        _result.add_gate("CX", qubit_list, Phase(1), true);
    }

    for(auto p: partitions){
        std::vector<Phase> phases = Phase_Polynomial::get_phase_of_terms(p);
        p.gaussian_elimination(true, false);
        auto cnots = p.get_row_operations();
        for(auto [ctrl, targ]: cnots){
            QubitIdList qubit_list;
            qubit_list.emplace_back(std::move(ctrl));
            qubit_list.emplace_back(std::move(targ));
            _result.add_gate("CX", qubit_list, Phase(1), true);
        }
        for(size_t i=0; i<phases.size(); i++){
            _result.add_single_rz(std::move(i), phases[i], true);
        }
        reverse(cnots.begin(), cnots.end());
        for(auto [ctrl, targ]: cnots){
            QubitIdList qubit_list;
            qubit_list.emplace_back(std::move(ctrl));
            qubit_list.emplace_back(std::move(targ));   
            _result.add_gate("CX", qubit_list, Phase(1), true);
        }
    }

    terminal_wires.gaussian_elimination(true, false);
    auto cnots_terminal = terminal_wires.get_row_operations();
    reverse(cnots_terminal.begin(), cnots_terminal.end());
    for(auto [ctrl, targ]: cnots_terminal){
        QubitIdList qubit_list;
        qubit_list.emplace_back(std::move(ctrl));
        qubit_list.emplace_back(std::move(targ));
        _result.add_gate("CX", qubit_list, Phase(1), true);
    }


    _result.print_gate_statistics();
    _result.print_circuit_diagram(spdlog::level::info);
};

/**
 * @brief Add h gate into circuit
 * @param size_t nth_h
 * 
 */
void Phase_Polynomial::add_H_gate(size_t nth_h){
    size_t q = _h[nth_h];
    QubitIdList qubit_list;
    qubit_list.emplace_back(std::move(q));
    
    _result.add_gate("H", qubit_list, Phase(0), true);
}


/**
 * @brief Get Phase of partition
 * @param Partition p
 * @return vector<Phase>
 */
std::vector<Phase> Phase_Polynomial::get_phase_of_terms(Partition p){
    std::vector<Phase> phases;
    for(size_t i=0; i<p.num_rows(); i++){
        size_t nth_row = _pp_terms.find_row(p.get_row(i)).value();
        Phase p = _pp_coeff[nth_row];
        phases.emplace_back(p);
    }
    return phases;
}

} // namespace qsyn::pp