/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Report the runtime and memory usage ]
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

    void report(bool report_time, bool report_mem);

private:
    // for Memory usage (in MB)
    double _initial_memory = 0.0;
    double _current_memory = 0.0;

    // for CPU time usage
    double _current_tick = 0.0;
    double _period_used_time = 0.0;
    double _total_used_time = 0.0;

    // private functions
    double _check_memory() const;
    double _check_tick() const;
    void _set_memory_usage();
    void _set_time_usage();
};

}  // namespace utils

}  // namespace dvlab