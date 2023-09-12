/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirQubit member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_qubit.hpp"

#include <iomanip>

#include "./qcir_gate.hpp"

namespace qsyn::qcir {

/**
 * @brief Print qubit info
 *
 */
void QCirQubit::print_qubit_line() const {
    QCirGate *current = _bit_first;
    size_t last_time = 1;
    std::cout << "Q" << std::right << std::setfill(' ') << std::setw(2) << _id << "  ";
    while (current != nullptr) {
        std::cout << "-";
        while (last_time < current->get_time()) {
            std::cout << "----";
            std::cout << "----";
            last_time++;
        }
        std::cout << std::setfill(' ') << std::setw(2) << current->get_type_str().substr(0, 2);
        std::cout << "(" << std::setfill(' ') << std::setw(2) << current->get_id() << ")";
        last_time = current->get_time() + 1;
        current = current->get_qubit(_id)._child;
        std::cout << "-";
    }
    std::cout << std::endl;
}

}  // namespace qsyn::qcir