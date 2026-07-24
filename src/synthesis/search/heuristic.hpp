/****************************************************************************
  PackageName  [ synthesis / search ]
  Synopsis     [ Heuristic interface: maps (residual, depth) -> a scalar
                 priority that orders the search frontier. Two concrete
                 implementations:
                   - GreedyHeuristic:   priority = residual
                   - AStarHeuristic:    priority = residual + alpha * depth
                                        (mirrors BQSKit's default A* heuristic
                                        with a weighting factor that penalises
                                        deeper candidates).

                 Heuristics intentionally do not own state -- the frontier
                 caches the priority in each Candidate, so a stateless
                 heuristic is sufficient. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>

namespace qsyn::synthesis::search {

class Heuristic {
public:
    virtual ~Heuristic() = default;
    [[nodiscard]] virtual double priority(double residual,
                                          std::size_t depth) const = 0;
};

class GreedyHeuristic final : public Heuristic {
public:
    [[nodiscard]] double priority(double residual, std::size_t /*depth*/) const override {
        return residual;
    }
};

class AStarHeuristic final : public Heuristic {
public:
    // alpha = depth-weight. BQSKit's default is 0.1 -- larger values
    // shrink the search tree at the cost of solution quality. We use
    // a slightly smaller default (0.05) because LBFGS instantiation
    // already drives residual much lower per layer than coord-descent
    // in BQSKit's upstream defaults.
    explicit AStarHeuristic(double alpha = 0.05) : _alpha(alpha) {}

    [[nodiscard]] double priority(double residual, std::size_t depth) const override {
        return residual + _alpha * static_cast<double>(depth);
    }

private:
    double _alpha;
};

}  // namespace qsyn::synthesis::search
