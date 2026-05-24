// SPDX-License-Identifier: MIT
#include "cppgridsynth/loop_controller.hpp"

namespace cppgridsynth {

LoopController::LoopController(int dloop, int floop,
                               double dtimeout_ms, double ftimeout_ms)
    : diophantine_loops_(dloop),
      factoring_loops_(floop),
      diophantine_timeout_s_(dtimeout_ms / 1000.0),
      factoring_timeout_s_(ftimeout_ms / 1000.0),
      d_counter_(0),
      f_counter_(0),
      d_started_(false),
      f_started_(false) {}

void LoopController::start_diophantine() {
    d_counter_ = 0;
    d_start_ = clock::now();
    d_started_ = true;
}

void LoopController::start_factoring() {
    f_counter_ = 0;
    f_start_ = clock::now();
    f_started_ = true;
}

bool LoopController::check_diophantine_continue() {
    if (d_counter_ >= diophantine_loops_) return false;
    if (std::isfinite(diophantine_timeout_s_) && d_started_) {
        double elapsed = std::chrono::duration<double>(clock::now() - d_start_).count();
        if (elapsed >= diophantine_timeout_s_) return false;
    }
    ++d_counter_;
    return true;
}

bool LoopController::check_factoring_continue() {
    if (f_counter_ >= factoring_loops_) return false;
    if (std::isfinite(factoring_timeout_s_) && f_started_) {
        double elapsed = std::chrono::duration<double>(clock::now() - f_start_).count();
        if (elapsed >= factoring_timeout_s_) return false;
    }
    ++f_counter_;
    return true;
}

void LoopController::reset_counters() {
    d_counter_ = 0;
    f_counter_ = 0;
    d_started_ = false;
    f_started_ = false;
}

}  // namespace cppgridsynth
