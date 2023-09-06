/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirQubit member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./qcir_qubit.hpp"

#include <iomanip>

#include "./qcir_gate.hpp"

using namespace std;

/**
 * @brief Print qubit info
 *
 */
void QCirQubit::print_qubit_line() const {
    QCirGate *current = _bit_first;
    size_t last_time = 1;
    cout << "Q" << right << setfill(' ') << setw(2) << _id << "  ";
    while (current != nullptr) {
        cout << "-";
        while (last_time < current->get_time()) {
            cout << "----";
            cout << "----";
            last_time++;
        }
        cout << setfill(' ') << setw(2) << current->get_type_str().substr(0, 2);
        cout << "(" << setfill(' ') << setw(2) << current->get_id() << ")";
        last_time = current->get_time() + 1;
        current = current->get_qubit(_id)._child;
        cout << "-";
    }
    cout << endl;
}