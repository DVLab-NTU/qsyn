/****************************************************************************
  FileName     [ qcirQubit.cpp ]
  PackageName  [ qcir ]
  Synopsis     [ Define qcir qubit functions ]
  Author       [ Chin-Yi Cheng ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <vector>
#include <string>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include "qcirMgr.h"
#include "qcirGate.h"
#include "qcirQubit.h"

using namespace std;

extern QCirMgr *qCirMgr;
extern size_t verbose;

void QCirQubit::printBitLine() const
{
  QCirGate *current = _bitFirst;
  size_t last_time = 0;
  cout << "Q" << right << setfill(' ') << setw(2) << _id << "  ";
  while (current != NULL)
  {
    cout << "-";
    while (last_time < current->getTime())
    {
      cout << "----";
      cout << "----";
      last_time++;
    }
    cout << setfill(' ') << setw(2) << current->getTypeStr().substr(0, 2);
    cout << "("<< setfill(' ') << setw(2) << current->getId()<<")";
    last_time = current->getTime() + 1;
    current = current->getQubit(_id)._child;
    cout << "-";
  }
  cout << endl;
}