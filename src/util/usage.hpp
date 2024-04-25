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
    Usage() : _initial_memory(_check_memory()), _current_memory(_initial_memory), _current_tick(_check_tick()), _period_used_time(0.0), _total_used_time(0.0) {}

    void reset_period();

    void start_tick();
    void end_tick();

    void report(bool report_time, bool report_mem);

private:
    // for Memory usage (in MiB)
    double _initial_memory = 0.0;
    double _current_memory;

    // for CPU time usage
    double _current_tick;
    double _period_used_time;
    double _total_used_time;

    // private functions
    double _check_memory();
    double _check_tick();
};

}  // namespace utils

}  // namespace dvlab
