/****************************************************************************
  PackageName  [ synthesis / search ]
  Synopsis     [ QSearch / LEAP frontier: a priority queue of (partial-
                 circuit, instantiated cost) pairs ordered by a pluggable
                 Heuristic. The driver pops the best candidate, expands
                 it via a LayerGenerator, instantiates each child via
                 the PR-B Minimizer, and pushes the children back onto
                 the frontier.

                 BQSKit reference: bqskit/passes/synthesis/qsearch.py
                 + bqskit/ir/frontier.py. The C++ port intentionally
                 mirrors the public API so future heuristics / layer
                 generators map 1:1. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2026 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <memory>
#include <queue>
#include <vector>

#include "qcir/qcir.hpp"
#include "tensor/qtensor.hpp"

namespace qsyn::synthesis::search {

// A leaf of the search tree: a partial ansatz plus its current
// residual (1 - cosine_similarity) against the target unitary.
struct Candidate {
    qcir::QCir circuit;
    double residual = 1.0;
    // Cost used by Heuristic to order the frontier. Computed once at
    // push-time and cached so the priority queue doesn't have to
    // re-evaluate the heuristic on each comparison.
    double priority = 0.0;
    // Depth = number of "layers" appended so far (1 layer == 1 CX
    // building block in the SimpleLayerGenerator). Used by LEAP and
    // various heuristics that penalise circuit depth.
    std::size_t depth = 0;
};

// Strict-weak ordering for std::priority_queue. We want the *smallest*
// priority at the top (lower = better), so we invert the comparison.
struct CandidateGreater {
    bool operator()(Candidate const& a, Candidate const& b) const {
        return a.priority > b.priority;
    }
};

class Frontier {
public:
    Frontier() = default;

    [[nodiscard]] bool empty() const { return _heap.empty(); }
    [[nodiscard]] std::size_t size() const { return _heap.size(); }

    void push(Candidate c) { _heap.push(std::move(c)); }

    [[nodiscard]] Candidate pop() {
        Candidate top = _heap.top();
        _heap.pop();
        return top;
    }

    [[nodiscard]] Candidate const& peek() const { return _heap.top(); }

private:
    std::priority_queue<Candidate, std::vector<Candidate>, CandidateGreater> _heap;
};

}  // namespace qsyn::synthesis::search
