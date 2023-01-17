/****************************************************************************
  FileName     [ myUsage.cpp ]
  PackageName  [ util ]
  Synopsis     [ Report the run time and memory usage ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2007-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/

#include "myUsage.h"

#include <iomanip>
#include <iostream>

using namespace std;

#undef MYCLK_TCK
#define MYCLK_TCK sysconf(_SC_CLK_TCK)

void MyUsage::reset() {
    _initMem = checkMem();
    _currentTick = checkTick();
    _periodUsedTime = _totalUsedTime = 0.0;
}

void MyUsage::report(bool repTime, bool repMem) {
    if (repTime) {
        setTimeUsage();
        cout << "Period time used : " << setprecision(4)
             << _periodUsedTime << " seconds" << endl;
        cout << "Total time used  : " << setprecision(4)
             << _totalUsedTime << " seconds" << endl;
    }
    if (repMem) {
        setMemUsage();
        cout << "Total memory used: " << setprecision(4)
             << _currentMem << " M Bytes" << endl;
    }
}
