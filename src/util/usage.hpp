/****************************************************************************
  FileName     [ usage.hpp ]
  PackageName  [ util ]
  Synopsis     [ Report the run time and memory usage ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

namespace dvlab_utils {

class Usage {
public:
    Usage() { reset(); }

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
    double checkMem() const;
    double checkTick() const;
    void setMemUsage();
    void setTimeUsage();
};

}  // namespace dvlab_utils
