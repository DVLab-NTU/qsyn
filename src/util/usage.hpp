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

    static void reset();

    static void report(bool report_time, bool report_mem);

private:
    // for Memory usage (in MB)
    static double _initial_memory;
    static double _current_memory;

    // for CPU time usage
    static double _current_tick;
    static double _period_used_time;
    static double _total_used_time;

    // private functions
    static double _check_memory();
    static double _check_tick();
    static void _set_memory_usage();
    static void _set_time_usage();
};

}  // namespace utils

}  // namespace dvlab