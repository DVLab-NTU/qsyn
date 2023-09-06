/****************************************************************************
  FileName     [ usage.hpp ]
  PackageName  [ util ]
  Synopsis     [ Report the run time and memory usage ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

namespace dvlab {

namespace utils {

class Usage {
public:
    Usage() { reset(); }

    void reset();

    void report(bool repTime, bool repMem);

private:
    // for Memory usage (in MB)
    double _initMem = 0.0;
    double _currentMem = 0.0;

    // for CPU time usage
    double _currentTick = 0.0;
    double _periodUsedTime = 0.0;
    double _totalUsedTime = 0.0;

    // private functions
    double checkMem() const;
    double checkTick() const;
    void setMemUsage();
    void setTimeUsage();
};

}  // namespace utils

}  // namespace dvlab