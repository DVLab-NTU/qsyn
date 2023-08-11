/****************************************************************************
  FileName     [ usage.cpp ]
  PackageName  [ util ]
  Synopsis     [ Report the run time and memory usage ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2007-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./usage.hpp"

#include <sys/resource.h>
#include <sys/times.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>

using namespace std;
using namespace dvlab_utils;

void Usage::reset() {
    _initMem = checkMem();
    _currentTick = checkTick();
    _periodUsedTime = _totalUsedTime = 0.0;
}

void Usage::report(bool repTime, bool repMem) {
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

double Usage::checkMem() const {
    struct rusage usage;
    if (0 == getrusage(RUSAGE_SELF, &usage))
#ifdef __APPLE__
        return usage.ru_maxrss / double(1 << 20);  // bytes
#else
        return usage.ru_maxrss / double(1 << 10);  // KBytes
#endif
    else
        return 0;
}
double Usage::checkTick() const {
    tms tBuffer;
    times(&tBuffer);
    return tBuffer.tms_utime;
}
void Usage::setMemUsage() { _currentMem = checkMem() - _initMem; }
void Usage::setTimeUsage() {
    double thisTick = checkTick();
    _periodUsedTime = (thisTick - _currentTick) / double(sysconf(_SC_CLK_TCK));
    _totalUsedTime += _periodUsedTime;
    _currentTick = thisTick;
}
