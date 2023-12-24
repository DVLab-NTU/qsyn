/****************************************************************************
  PackageName  [ pp ]
  Synopsis     [ Implement resynthesis algorithm ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "pp.hpp"

#include "util/boolean_matrix.hpp"
#include "qcir/qcir.hpp"

#include <iostream>


using namespace qsyn::qcir;
using namespace std;

namespace qsyn::pp {
/**
 * @brief Preform gaussian to resynthesis
 * @param Partitions p
 * @param HMAP h_map
 * @param size_t n: data qubit number
 * @param size_t a: acilla qubit number
 * @return QCir* 
 */
void Phase_Polynomial::gaussian_resynthesis(std::vector<dvlab::BooleanMatrix> partitions) {
    cout << "Into resynthesis" << endl;
    for_each(p.begin(), p.end(), [&](dvlab::BooleanMatrix m){cout<<"partition"<<endl;m.print_matrix();});

    for(auto p: partitions){}


};



} // namespace qsyn::pp