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
    virtual bool apply(ZXGraph& graph) = 0;
    // should return true if and only if the rule is successfully undone
    virtual bool undo(ZXGraph& graph) = 0;
};

/**
 * @brief Remove a Z/X-spider vertex with zero phase and exactly two neighbors
 *
 */
class IdentityRemoval : public ZXRule {
public:
    IdentityRemoval(size_t v_id);
    bool apply(ZXGraph& graph) override;
    bool undo(ZXGraph& graph) override;

private:
    size_t _v_id;
    size_t _left_id         = 0;
    size_t _right_id        = 0;
    VertexType _vtype       = VertexType::z;
    EdgeType _etype_to_left = EdgeType::hadamard;
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

    bool apply(ZXGraph& graph) override;
    bool undo(ZXGraph& graph) override;

private:
    size_t _left_id;
    size_t _right_id;
    VertexType _vtype;
    EdgeType _etype_to_left;
    size_t _new_v_id = 0;
};

}  // namespace qsyn::zx
