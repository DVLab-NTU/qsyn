// SPDX-License-Identifier: MIT
//
// Iteration / timeout governor for the Diophantine equation solver.
#pragma once

#include <chrono>
#include <limits>

namespace cppgridsynth {

class LoopController {
public:
    using clock = std::chrono::steady_clock;

    LoopController(int dloop = 10,
                   int floop = 10,
                   double dtimeout_ms = std::numeric_limits<double>::infinity(),
                   double ftimeout_ms = std::numeric_limits<double>::infinity());

    void start_diophantine();
    void start_factoring();

    bool check_diophantine_continue();
    bool check_factoring_continue();

    int diophantine_iteration_count() const { return d_counter_; }
    int factoring_iteration_count()   const { return f_counter_; }
    void reset_counters();

private:
    int diophantine_loops_;
    int factoring_loops_;
    double diophantine_timeout_s_;  // seconds (or inf)
    double factoring_timeout_s_;

    int d_counter_;
    int f_counter_;
    clock::time_point d_start_;
    clock::time_point f_start_;
    bool d_started_;
    bool f_started_;
};

}  // namespace cppgridsynth
