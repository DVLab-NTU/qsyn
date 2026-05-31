#ifdef QSYN_ENABLE_GRIDSYNTH

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

#include "qcir/gridsynth/gridsynth_adapter.hpp"
#include "util/phase.hpp"

using dvlab::Phase;
namespace gsd = qsyn::qcir::gridsynth_detail;

namespace {

gsd::GridsynthRequest make_request(Phase phase, std::string epsilon, int seed = 0) {
    auto const& r = phase.get_rational();
    gsd::GridsynthRequest req;
    req.theta_numer = std::to_string(r.numerator());
    req.theta_denom = std::to_string(r.denominator());
    req.epsilon     = std::move(epsilon);
    req.seed        = seed;
    return req;
}

size_t count_t_gates(std::vector<gsd::SynthGate> const& gates) {
    return static_cast<size_t>(std::ranges::count_if(
        gates, [](gsd::SynthGate const& g) {
            return g.kind == gsd::SynthGateKind::T;
        }));
}

// GridSynth (Selinger–Ross): T-count is O(log(1/ε)) with leading constant 3.
size_t theoretical_t_upper_bound(double epsilon, size_t slack = 8) {
    return static_cast<size_t>(std::ceil(3.0 * std::log2(1.0 / epsilon))) + slack;
}

}  // namespace

TEST_CASE("GridSynth T-count stays within 3 log2(1/eps) bound", "[qcir][gridsynth][tcount]") {
    auto const epsilon_str = GENERATE(
        std::string("1e-4"),
        std::string("1e-6"),
        std::string("1e-8"),
        std::string("1e-10"));

    auto const theta = GENERATE(
        Phase(1, 8),
        Phase(1, 12),
        Phase(1, 16),
        Phase(1, 32));

    double const epsilon = std::stod(epsilon_str);
    auto const gates     = gsd::synthesize_rz(make_request(theta, epsilon_str));
    REQUIRE(gates.has_value());
    auto const t_count = count_t_gates(*gates);
    auto const bound     = theoretical_t_upper_bound(epsilon);

    INFO("epsilon=" << epsilon_str << " theta=" << theta.get_print_string()
                    << " T=" << t_count << " bound=" << bound);

    REQUIRE(t_count <= bound);
    REQUIRE(t_count >= 1);
}

TEST_CASE("GridSynth T-count grows with tighter epsilon", "[qcir][gridsynth][tcount]") {
    Phase const theta(1, 8);
    size_t prev_t = 0;

    for (auto const* eps : {"1e-4", "1e-6", "1e-8", "1e-10"}) {
        auto const gates = gsd::synthesize_rz(make_request(theta, eps));
        REQUIRE(gates.has_value());
        auto const t = count_t_gates(*gates);
        REQUIRE(t >= prev_t);
        prev_t = t;
    }
}

TEST_CASE("GridSynth synthesis completes within reasonable time", "[qcir][gridsynth][perf]") {
    constexpr int iterations = 100;

    Phase const theta(1, 8);

    auto const start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto const gates = gsd::synthesize_rz(make_request(theta, "1e-8", i));
        REQUIRE(gates.has_value());
    }
    auto const ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();

    INFO("100 syntheses (ε=1e-8) took " << ms << " ms");
    REQUIRE(ms < 30'000);
}

TEST_CASE("GridSynth repeated synthesis is stable", "[qcir][gridsynth][perf]") {
    // Run many iterations to catch crashes; pair with ASan builds for leak detection.
    constexpr int iterations = 200;

    Phase const theta(1, 64);

    for (int i = 0; i < iterations; ++i) {
        auto const gates = gsd::synthesize_rz(make_request(theta, "1e-6", i));
        REQUIRE(gates.has_value());
        REQUIRE(count_t_gates(*gates) > 0);
    }
}

#endif  // QSYN_ENABLE_GRIDSYNTH
