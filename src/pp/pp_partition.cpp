/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Implement partition algorithm ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "pp_partition.hpp"

#include <iostream>
#include <iostream>  // to delete

#include "qcir/qcir.hpp"
#include "util/boolean_matrix.hpp"

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
void Partitioning::initial(dvlab::BooleanMatrix poly, size_t n, size_t a) {
    _qubit_num = n + a;
    _poly      = poly;
    _variable  = n;
}

/**
 * @brief Verify if satisfy Lemma: dim(V) - rank(S) <= n - |S|
 * @param Partition S
 * @return true if satified, ow, false
 */
bool Partitioning::independant_oracle(Partition S, term t) {
    Partition temp = S;
    temp.push_row(t);
    size_t rank = temp.gaussian_elimination_skip(temp.num_cols(), true);
    cout << "Rank is " << rank << ". Return " << (_variable - rank <= _qubit_num - temp.num_rows()) << endl;
    return (_variable - rank <= _qubit_num - temp.num_rows());
}

/**
 * @brief Greedy partitioning
 * @param Wires w
 * @return Partitions
 */
Partitions Partitioning::greedy_partitioning(HMAP h_map, size_t rank) {
    Partitions partitions;
    for (auto [wires, q] : h_map) {
        Partitioning::greedy_partitioning_routine(partitions, wires, rank);
    }
    if (_poly.num_rows() != 0) _poly.print_matrix();
    assert(_poly.num_rows() == 0);
    return partitions;
}

/**
 * @brief Greedy partitioning routine call by Partitioning::greedy_partitioning
 * @param Partitions partitions: partitions to be added
 * @param Wires w: wires can be use now
 * @param size_t rank: rank of the wires (=data qubit number)
 */
Partitions Partitioning::greedy_partitioning_routine(Partitions total_partitions, Wires wires, size_t rank) {
    Partitions partitions;
    std::vector<size_t> partitioned;

    auto is_constructable = [&](term t) {
        Wires temp = wires;
        temp.push_row(t);
        return (rank == temp.gaussian_elimination_skip(temp.num_cols(), true));
    };
    size_t flag = 0;
    for (size_t i = 0; i < _poly.num_rows(); i++) {
        term r = _poly.get_row(i);
        cout << "Try to partition ";
        r.print_row();
        // Wires temp = wires;
        // temp.push_row(r);
        // cout << "New term rank is " << temp.gaussian_elimination_skip(temp.num_cols(), true) << endl;
        if (!is_constructable(r)) continue;
        // cout << "Is constructable" << endl;
        partitioned.emplace(partitioned.begin(), i);
        if (partitions.size()==0){
            Partition p;
            p.push_row(r);
            partitions.push_back(p);
            continue;
        }
        bool need_new_p = true;
        for (size_t j = flag; j<partitions.size(); j++){
            if (Partitioning::independant_oracle(partitions[j], r)){
                partitions[j].push_row(r);
                need_new_p = false;
                if(partitions[j].num_rows() == wires.num_rows()) flag++;
                break;
            }
        }
        if (need_new_p) {
            Partition p;
            p.push_row(r);
            partitions.push_back(p);
        }
    }

    total_partitions.insert(total_partitions.begin(), partitions.begin(), partitions.end());
    cout << "Before size: " << _poly.num_rows() << endl;
    for_each(partitioned.begin(), partitioned.end(), [&](size_t i) { _poly.erase_row(i); });
    cout << "After size: " << _poly.num_rows() << endl;
    return partitions;
}

}  // namespace qsyn::pp
