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
double Usage::INITIAL_MEMORY = 0.0;
double Usage::CURRENT_MEMORY = 0.0;

// for CPU time usage
double Usage::CURRENT_TICK     = 0.0;
double Usage::PERIOD_USED_TIME = 0.0;
double Usage::TOTAL_USED_TIME  = 0.0;

void Usage::reset() {
    INITIAL_MEMORY   = Usage::_check_memory();
    CURRENT_TICK     = Usage::_check_tick();
    PERIOD_USED_TIME = TOTAL_USED_TIME = 0.0;
}

void Usage::report(bool report_time, bool report_mem) {
    if (report_time) {
        _set_time_usage();
        fmt::println("Period time used : {:.4f} seconds", PERIOD_USED_TIME);
        fmt::println("Total time used  : {:.4f} seconds", TOTAL_USED_TIME);
    }
    if (report_mem) {
        _set_memory_usage();
        fmt::println("Total memory used: {:.4f} M Bytes", CURRENT_MEMORY);
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
void Usage::_set_memory_usage() { CURRENT_MEMORY = _check_memory() - INITIAL_MEMORY; }
void Usage::_set_time_usage() {
    double const this_tick = _check_tick();
    PERIOD_USED_TIME       = (this_tick - CURRENT_TICK) / double(sysconf(_SC_CLK_TCK));
    TOTAL_USED_TIME += PERIOD_USED_TIME;
    CURRENT_TICK = this_tick;
}
