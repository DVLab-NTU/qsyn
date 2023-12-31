/****************************************************************************
  PackageName  [ util ]
  Synopsis     [ Report the runtime and memory usage ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./usage.hpp"

#include <fmt/core.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <unistd.h>

#include <gsl/narrow>

using namespace dvlab::utils;

// for Memory usage (in MB)
double Usage::_initial_memory = 0.0;
double Usage::_current_memory = 0.0;

// for CPU time usage
double Usage::_current_tick     = 0.0;
double Usage::_period_used_time = 0.0;
double Usage::_total_used_time  = 0.0;

void Usage::reset() {
    _initial_memory   = Usage::_check_memory();
    _current_tick     = Usage::_check_tick();
    _period_used_time = _total_used_time = 0.0;
}

void Usage::report(bool report_time, bool report_mem) {
    if (report_time) {
        _set_time_usage();
        fmt::println("Period time used : {:.4f} seconds", _period_used_time);
        fmt::println("Total time used  : {:.4f} seconds", _total_used_time);
    }
    if (report_mem) {
        _set_memory_usage();
        fmt::println("Total memory used: {:.4f} M Bytes", _current_memory);
    }
}

double Usage::_check_memory() {
    rusage usage{};
    if (0 == getrusage(RUSAGE_SELF, &usage))
        return gsl::narrow_cast<double>(usage.ru_maxrss) / double(1 << 10);  // KBytes // NOLINT(cppcoreguidelines-pro-type-union-access) : conform to POSIX
    else
        return 0;
}
double Usage::_check_tick() {
    tms buffer{};
    times(&buffer);
    return gsl::narrow_cast<double>(buffer.tms_utime);
}
void Usage::_set_memory_usage() { _current_memory = _check_memory() - _initial_memory; }
void Usage::_set_time_usage() {
    double const this_tick = _check_tick();
    _period_used_time      = (this_tick - _current_tick) / double(sysconf(_SC_CLK_TCK));
    _total_used_time += _period_used_time;
    _current_tick = this_tick;
}
