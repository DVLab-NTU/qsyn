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
    static double INITIAL_MEMORY;
    static double CURRENT_MEMORY;

    // for CPU time usage
    static double CURRENT_TICK;
    static double PERIOD_USED_TIME;
    static double TOTAL_USED_TIME;

    // private functions
    static double _check_memory();
    static double _check_tick();
    static void _set_memory_usage();
    static void _set_time_usage();
};

}  // namespace utils

}  // namespace dvlab
