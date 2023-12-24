/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Implement partition algorithm ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#pragma once

#include "util/boolean_matrix.hpp"

namespace qsyn::pp{

using Partition   = dvlab::BooleanMatrix;
using term        = dvlab::BooleanMatrix::Row;
using Partitions  = std::vector<dvlab::BooleanMatrix>;
using Wires       = dvlab::BooleanMatrix;
using HMAP        = std::vector<std::pair<dvlab::BooleanMatrix, size_t>>;

class Partitioning{

public:

    Partitioning () {};
    Partitioning (dvlab::BooleanMatrix poly, size_t n, size_t a) {Partitioning::initial(poly, n, a);};

    void initial(dvlab::BooleanMatrix poly, size_t n, size_t a);
    bool independant_oracle(Partition, term);

    // Greedy partitiion
    Partitions greedy_partitioning(HMAP h_map, size_t rank);
    void greedy_partitioning_routine(Partitions partitions, Wires wires, size_t rank);

    // Matroid partition


    // Get function


    // Print function 

private:
    size_t _variable;
    size_t _qubit_num;
    Partitions _partitions;
    dvlab::BooleanMatrix _poly;

};



} // namespace qsyn::pp
