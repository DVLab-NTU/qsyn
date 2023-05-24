/****************************************************************************
  FileName     [ myUsage.h ]
  PackageName  [ util ]
  Synopsis     [ Report the run time and memory usage ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef MY_USAGE_H
#define MY_USAGE_H

#include <sys/resource.h>  // for getrusage, RUSAGE_SELF
#include <sys/times.h>     // for times, tms
#include <unistd.h>        // for sysconf, _SC_CLK_TCK

#undef MYCLK_TCK
#define MYCLK_TCK sysconf(_SC_CLK_TCK)

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
        _periodUsedTime = (thisTick - _currentTick) / double(MYCLK_TCK);
        _totalUsedTime += _periodUsedTime;
        _currentTick = thisTick;
    }
};

#endif  // MY_USAGE_H
