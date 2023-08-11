/****************************************************************************
  FileName     [ myUsage.h ]
  PackageName  [ util ]
  Synopsis     [ Report the run time and memory usage ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <sys/resource.h>
#include <sys/times.h>
#include <unistd.h>

class MyUsage {
public:
    MyUsage() { reset(); }

    void reset();

    void report(bool repTime, bool repMem);

private:
    // for Memory usage (in MB)
    double _initMem;
    double _currentMem;

    // for CPU time usage
    double _currentTick;
    double _periodUsedTime;
    double _totalUsedTime;

    // private functions
    double checkMem() const {
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
    double checkTick() const {
        tms tBuffer;
        times(&tBuffer);
        return tBuffer.tms_utime;
    }
    void setMemUsage() { _currentMem = checkMem() - _initMem; }
    void setTimeUsage() {
        double thisTick = checkTick();
        _periodUsedTime = (thisTick - _currentTick) / double(sysconf(_SC_CLK_TCK));
        _totalUsedTime += _periodUsedTime;
        _currentTick = thisTick;
    }
};
