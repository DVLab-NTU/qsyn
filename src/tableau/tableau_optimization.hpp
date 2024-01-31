/**
 * @file
 * @brief implementation of the tableau optimization
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#pragma once

#include "./stabilizer_tableau.hpp"

namespace qsyn {

namespace experimental {

void merge_rotations(std::vector<PauliRotation>& rotation);
void merge_rotations(StabilizerTableau& clifford, std::vector<PauliRotation>& rotations);

}  // namespace experimental

}  // namespace qsyn
