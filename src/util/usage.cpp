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

void Usage::reset_period() {
    _period_used_time = 0.0;
}

void Usage::start_tick() { _current_tick = _check_tick(); }

void Usage::end_tick() {
    auto const this_tick = _check_tick();
    auto const period    = (this_tick - _current_tick) / double(sysconf(_SC_CLK_TCK));
    _period_used_time += period;
    _total_used_time += period;
    _current_memory = _check_memory() - _initial_memory;
}

void Usage::report(bool report_time, bool report_mem) {
    if (report_time) {
        fmt::println("Period time used : {:.4f} seconds", _period_used_time);
        fmt::println("Total time used  : {:.4f} seconds", _total_used_time);
        reset_period();
    }
    if (report_mem) {
        fmt::println("Total memory used: {:.4f} MiBs", _current_memory);
    }
}

double Usage::_check_memory() {
    rusage usage{};
    if (0 == getrusage(RUSAGE_SELF, &usage))
#ifdef __APPLE__
        // macOS reports memory usage in bytes
        return static_cast<double>(usage.ru_maxrss) / double(1 << 20);  // MiB // NOLINT(cppcoreguidelines-pro-type-union-access) : conform to POSIX
#else
        // Linux reports memory usage in kibibytes
        return static_cast<double>(usage.ru_maxrss) / double(1 << 10);  // KiB // NOLINT(cppcoreguidelines-pro-type-union-access) : conform to POSIX
#endif
    else
        return 0;
}
double Usage::_check_tick() {
    tms buffer{};
    times(&buffer);
    return static_cast<double>(buffer.tms_utime);
}
