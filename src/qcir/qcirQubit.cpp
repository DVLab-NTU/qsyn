/****************************************************************************
  FileName     [ qcirQubit.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define class QCirQubit member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcirQubit.h"

#include "qcirGate.h"  // for QCirGate, BitInfo
class QCir;

using namespace std;

extern QCir *qCir;
extern size_t verbose;

/**
 * @brief Print qubit info
 *
 */
void QCirQubit::printBitLine() const {
    QCirGate *current = _bitFirst;
    size_t last_time = 1;
    cout << "Q" << right << setfill(' ') << setw(2) << _id << "  ";
    while (current != NULL) {
        cout << "-";
        while (last_time < current->getTime()) {
            cout << "----";
            cout << "----";
            last_time++;
        }
        cout << setfill(' ') << setw(2) << current->getTypeStr().substr(0, 2);
        cout << "(" << setfill(' ') << setw(2) << current->getId() << ")";
        last_time = current->getTime() + 1;
        current = current->getQubit(_id)._child;
        cout << "-";
    }
    cout << endl;
}