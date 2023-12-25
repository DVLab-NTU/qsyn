/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Implement resynthesis algorithm ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <spdlog/spdlog.h>
#include "pp.hpp"
#include "pp_partition.hpp"

#include "util/boolean_matrix.hpp"
#include "qcir/qcir.hpp"

#include <cassert>
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
    // cout << "Into resynthesis" << endl;
    // cout << "initial wires" << endl;
    // initial_wires.print_matrix();
    // cout << "terminal wires" << endl;
    // terminal_wires.print_matrix();
    
    // for_each(partitions.begin(), partitions.end(), [&](dvlab::BooleanMatrix m){cout<<"partition"<<endl;m.print_matrix();});


    initial_wires.gaussian_elimination_skip(true, true, true);
    auto cnots_initial = initial_wires.get_row_operations();
    for(auto [ctrl, targ]: cnots_initial){
        QubitIdList qubit_list;
        qubit_list.emplace_back(std::move(ctrl));
        qubit_list.emplace_back(std::move(targ));
        _result.add_gate("CX", qubit_list, Phase(1), true);
    }

    for(auto p: partitions){
        std::vector<Phase> phases = Phase_Polynomial::get_phase_of_terms(p);
        Partition complete_p = Phase_Polynomial::complete_the_partition(initial_wires, p);
        // cout << "complete_p" << endl;
        // complete_p.print_matrix();
        complete_p.gaussian_elimination_skip(complete_p.num_cols(), true, true);
        // complete_p.print_trace();
        auto cnots = complete_p.get_row_operations();
        reverse(cnots.begin(), cnots.end());
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

    terminal_wires.gaussian_elimination_skip(terminal_wires.num_cols(), true, true);

    auto cnots_terminal = terminal_wires.get_row_operations();
    reverse(cnots_terminal.begin(), cnots_terminal.end());
    for(auto [ctrl, targ]: cnots_terminal){
        QubitIdList qubit_list;
        qubit_list.emplace_back(std::move(ctrl));
        qubit_list.emplace_back(std::move(targ));
        _result.add_gate("CX", qubit_list, Phase(1), true);
    }

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
 * @brief Complete the partition until size = wires
 * @param Wires: wires
 * @param Partition p
 * @return vector<Phase>
 */
Partition Phase_Polynomial::complete_the_partition(Wires wires, Partition partition){
    while(partition.num_rows() < wires.num_rows()){
        partition.push_row(wires.get_row(partition.num_rows()));
    }
    return partition;
}

/**
 * @brief Get Phase of partition
 * @param Partition p
 * @return vector<Phase>
 */
std::vector<Phase> Phase_Polynomial::get_phase_of_terms(Partition partition){
    std::vector<Phase> phases;
    for(size_t i=0; i<partition.num_rows(); i++){
        size_t nth_row = _pp_terms.find_row(partition.get_row(i)).value();
        Phase p = _pp_coeff[nth_row];
        phases.emplace_back(p);
        assert(_pp_terms[nth_row] == partition[i]);
    }
    return phases;
}

} // namespace qsyn::pp