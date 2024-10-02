/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph Action functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include "./zxgraph.hpp"
#include "zx/zx_def.hpp"

namespace qsyn::zx {

void toggle_vertex(ZXGraph& graph, size_t v_id);

std::optional<size_t> add_identity_vertex(
    ZXGraph& graph, size_t left_id, size_t right_id,
    VertexType vtype,
    EdgeType etype_to_left,
    std::optional<size_t> new_v_id = std::nullopt);

std::optional<std::tuple<size_t, size_t, VertexType, EdgeType>>
remove_identity_vertex(ZXGraph& graph, size_t v_id);

void gadgetize_phase(
    ZXGraph& graph, size_t v_id, Phase const& keep_phase = Phase(0));

/**
 * @brief ZX calculus rule interface.
 *
 */
class ZXRule {
public:
    virtual ~ZXRule() = default;

    // should return true if and only if the rule is successfully applied
    virtual bool apply(ZXGraph& graph) const = 0;
    // should return true if and only if the rule is successfully undone
    virtual bool undo(ZXGraph& graph) const = 0;
};

/**
 * @brief Remove a Z/X-spider vertex with zero phase and exactly two neighbors
 *
 */
class IdentityRemoval : public ZXRule {
public:
    IdentityRemoval(size_t v_id);
    bool apply(ZXGraph& graph) const override;
    bool undo(ZXGraph& graph) const override;

private:
    size_t _v_id;
    mutable size_t _left_id         = 0;
    mutable size_t _right_id        = 0;
    mutable VertexType _vtype       = VertexType::z;
    mutable EdgeType _etype_to_left = EdgeType::hadamard;
};

/**
 * @brief Add a Z/X-spider vertex with zero phase on an edge
 *
 */
class IdentityAddition : public ZXRule {
public:
    IdentityAddition(size_t left_id,
                     size_t right_id,
                     VertexType vtype,
                     EdgeType etype_to_left);

    bool apply(ZXGraph& graph) const override;
    bool undo(ZXGraph& graph) const override;

private:
    size_t _left_id;
    size_t _right_id;
    VertexType _vtype;
    EdgeType _etype_to_left;
    mutable size_t _new_v_id = 0;
};

// class SpiderFusion : public ZXRule {
// public:
//     SpiderFusion(size_t v0_id, size_t v1_id);
//     bool apply(ZXGraph& graph) const override;
//     bool undo(ZXGraph& graph) const override;

// private:
//     size_t _v0_id;
//     size_t _v1_id;
//     mutable VertexType _v1_type = VertexType::z;
//     mutable Phase _v1_phase;
//     mutable std::vector<size_t> _v1_neighbors;
// };

/**
 * @brief Remove a identity vertex and merge its neighbors.
 *        Assumes the graph is graph-like.
 *
 */
class IdentityFusion : public ZXRule {
public:
    IdentityFusion(size_t v_id);
    bool apply(ZXGraph& graph) const override;
    bool undo(ZXGraph& graph) const override;

private:
    size_t _v_id;
    mutable size_t _left_id    = 0;
    mutable size_t _right_id   = 0;
    mutable Phase _right_phase = Phase(0);
    mutable std::vector<size_t> _right_neighbors;
};

}  // namespace qsyn::zx
