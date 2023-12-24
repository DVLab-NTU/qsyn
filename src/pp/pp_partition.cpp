/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Implement partition algorithm ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#include "pp_partition.hpp"

#include <iostream>
#include "qcir/qcir.hpp"
#include "util/boolean_matrix.hpp"

#include <iostream> // to delete

using namespace qsyn::qcir;
using namespace std;

namespace qsyn::pp {

/**
 * @brief Set initial partition / graph
 * @param dvlab::BooleanMatrix poly
 * @param n: data qubit number
 * @param a: ancilla number
 * 
 */
void Partitioning::initial(dvlab::BooleanMatrix poly, size_t n, size_t a){
    _qubit_num = n+a;
    _poly = poly;
    _variable = n;
}

/**
 * @brief Verify if satisfy Lemma: dim(V) - rank(S) <= n - |S|
 * @param Partition S
 * @return true if satified, ow, false
 */
bool Partitioning::independant_oracle(Partition S, term t){
    Partition temp = S;
    temp.push_row(t);
    size_t rank = temp.gaussian_elimination_skip(temp.num_cols(), false);
    cout << "Rank is " << rank << ". Return " << (_variable-rank <= _qubit_num - temp.num_rows()) << endl;
    return (_variable - rank <= _qubit_num - temp.num_rows());
}


/**
 * @brief Greedy partitioning
 * @param Wires w
 * @return Partitions
 */
Partitions Partitioning::greedy_partitioning(HMAP h_map, size_t rank){
    Partitions partitions;
    for(auto [wires, q]: h_map){
      Partitioning::greedy_partitioning_routine(partitions, wires, rank);
    }
    assert(_poly.num_rows()==0);
    return partitions;
}

/**
 * @brief Greedy partitioning routine call by Partitioning::greedy_partitioning
 * @param Partitions partitions: partitions to be added
 * @param Wires w: wires can be use now
 * @param size_t rank: rank of the wires (=data qubit number)
 */
void Partitioning::greedy_partitioning_routine(Partitions partitions, Wires wires, size_t rank){
    Partition p;
    std::vector<size_t> partitioned;

    cout << "Rank is " << rank << endl;
    cout << "Wires rank is " << wires.gaussian_elimination_skip(wires.num_cols(), false) << endl;
    wires.print_matrix();
    
    auto is_constructable = [&](term t){
        Wires temp = wires;
        temp.push_row(t);
        return (rank == temp.gaussian_elimination_skip(temp.num_cols(), false));
    };

    for(size_t i=0; i< _poly.num_rows(); i++){
        term r = _poly.get_row(i);
        Wires temp = wires;
        temp.push_row(r);
        cout << "New term rank is " << temp.gaussian_elimination_skip(temp.num_cols(), false) << endl;
        if (!is_constructable(r)) continue;
        cout << "Is constructable" << endl;
        if (p.num_rows()==0){
          p.push_row(r);
          continue;
        } 
        if (Partitioning::independant_oracle(p, r)) p.push_row(r);
        if (p.num_rows() == _qubit_num){
          partitions.push_back(p); // copy
          p.clear();
          partitioned.emplace(partitioned.begin(), i);
        }
    }

    for_each(partitioned.begin(), partitioned.end(), [&](size_t i){ _poly.erase_row(i);});
}

} // namespace qsyn::pp
