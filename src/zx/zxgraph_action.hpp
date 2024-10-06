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

    // should return true if and only if the rule is applicable to the graph
    virtual bool is_applicable(ZXGraph const& graph) const = 0;
    // should return true if and only if the rule is undoable
    virtual bool is_undoable(ZXGraph const& graph) const = 0;
    // should apply the rule without checking
    // should set the internal state of the rule class
    // so that undo_unchecked can be called
    virtual void apply_unchecked(ZXGraph& graph) const = 0;
    // should undo the rule without checking
    // should set the internal state of the rule class
    // so that apply_unchecked can be called
    virtual void undo_unchecked(ZXGraph& graph) const = 0;

    // returns true if and only if the rule is successfully applied
    bool apply(ZXGraph& graph, bool check = true) const {
        if (check && !is_applicable(graph)) return false;
        apply_unchecked(graph);
        return true;
    }
    // should return true if and only if the rule is successfully undone
    bool undo(ZXGraph& graph) const {
        if (!is_undoable(graph)) return false;
        undo_unchecked(graph);
        return true;
    }
};

/**
 * @brief Remove a Z/X-spider vertex with zero phase and exactly two neighbors
 *
 */
class IdentityRemoval : public ZXRule {
public:
    IdentityRemoval(size_t v_id);
    bool is_applicable(ZXGraph const& graph) const override;
    bool is_undoable(ZXGraph const& graph) const override;
    void apply_unchecked(ZXGraph& graph) const override;
    void undo_unchecked(ZXGraph& graph) const override;

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

    bool is_applicable(ZXGraph const& graph) const override;
    bool is_undoable(ZXGraph const& graph) const override;
    void apply_unchecked(ZXGraph& graph) const override;
    void undo_unchecked(ZXGraph& graph) const override;

private:
    size_t _left_id;
    size_t _right_id;
    VertexType _vtype;
    EdgeType _etype_to_left;
    mutable size_t _new_v_id = 0;
};

/**
 * @brief Remove a identity vertex and merge its neighbors.
 *        Identity fusion preserves the graph-like property.
 *
 */
class IdentityFusion : public ZXRule {
public:
    IdentityFusion(size_t v_id);
    bool is_applicable(ZXGraph const& graph) const override;
    bool is_undoable(ZXGraph const& graph) const override;
    void apply_unchecked(ZXGraph& graph) const override;
    void undo_unchecked(ZXGraph& graph) const override;

private:
    size_t _v_id;
    mutable size_t _left_id  = 0;
    mutable size_t _right_id = 0;
    mutable Phase _right_phase;
    mutable std::vector<size_t> _right_neighbors;
};

/**
 * @brief Detach a vertex from any boundary vertex neighbors by inserting
 *        Z-spider buffers. The buffer will be connected to the selected vertex
 *        via Hadamard edges. This rule preserves the graph-like property and
 *        serves as a helper rule for other rules such as LComp or pivoting.
 *
 */
class BoundaryDetachment : public ZXRule {
public:
    BoundaryDetachment(size_t v_id);
    bool is_applicable(ZXGraph const& graph) const override;
    bool is_undoable(ZXGraph const& graph) const override;
    void apply_unchecked(ZXGraph& graph) const override;
    void undo_unchecked(ZXGraph& graph) const override;

    auto const& get_buffers() const { return _buffers; }

private:
    size_t _v_id;
    mutable std::optional<std::vector<size_t>> _boundaries;
    mutable std::optional<std::vector<size_t>> _buffers;
};

/**
 * @brief Apply the LComp rule about a vertex. The vertex should be a Z-spider
 *        with phase ±π/2. LComp preserves the graph-like property.
 *
 */
class LComp : public ZXRule {
public:
    LComp(size_t v_id);
    bool is_applicable(ZXGraph const& graph) const override;
    bool is_undoable(ZXGraph const& graph) const override;
    void apply_unchecked(ZXGraph& graph) const override;
    void undo_unchecked(ZXGraph& graph) const override;

    auto get_v_id() const { return _v_id; }
    auto get_num_neighbors() const { return _neighbors.size(); }

    bool is_applicable_no_phase_check(ZXGraph const& graph) const;

private:
    size_t _v_id;
    mutable Phase _v_phase;
    mutable std::optional<BoundaryDetachment> _bd = std::nullopt;
    mutable std::vector<size_t> _neighbors;

    void _complement_neighbors(ZXGraph& graph) const;
};

class Pivot : public ZXRule {
public:
    Pivot(size_t v1_id, size_t v2_id);
    bool is_applicable(ZXGraph const& graph) const override;
    bool is_undoable(ZXGraph const& graph) const override;
    void apply_unchecked(ZXGraph& graph) const override;
    void undo_unchecked(ZXGraph& graph) const override;

    bool is_applicable_no_phase_check(ZXGraph const& graph) const;

    auto get_num_v1_neighbors() const { return _v1_neighbors.size(); }
    auto get_num_v2_neighbors() const { return _v2_neighbors.size(); }
    auto get_num_both_neighbors() const { return _both_neighbors.size(); }

private:
    size_t _v1_id;
    size_t _v2_id;
    mutable Phase _v1_phase;
    mutable Phase _v2_phase;
    mutable std::optional<BoundaryDetachment> _bd1 = std::nullopt;
    mutable std::optional<BoundaryDetachment> _bd2 = std::nullopt;
    mutable std::vector<size_t> _v1_neighbors;
    mutable std::vector<size_t> _v2_neighbors;
    mutable std::vector<size_t> _both_neighbors;

    void _complement_neighbors(ZXGraph& graph) const;
    void _adjust_phases(ZXGraph& graph) const;
};

class NeighborUnfusion : public ZXRule {
public:
    NeighborUnfusion(size_t v_id,
                     Phase phase_to_keep,
                     std::vector<size_t> const& neighbors_to_unfuse);
    bool is_applicable(ZXGraph const& graph) const override;
    bool is_undoable(ZXGraph const& graph) const override;
    void apply_unchecked(ZXGraph& graph) const override;
    void undo_unchecked(ZXGraph& graph) const override;

    auto get_v_id() const { return _v_id; }
    auto get_buffer_id() const { return _buffer_v_id; }
    auto get_unfused_id() const { return _unfused_v_id; }
    auto get_neighbors_to_unfuse() const { return _neighbors_to_unfuse; }

private:
    size_t _v_id;
    Phase _phase_to_keep;
    std::vector<size_t> _neighbors_to_unfuse;
    mutable std::optional<size_t> _buffer_v_id  = std::nullopt;
    mutable std::optional<size_t> _unfused_v_id = std::nullopt;
};

class LCompUnfusion : public ZXRule {
public:
    LCompUnfusion(size_t v_id, std::vector<size_t> const& neighbors_to_unfuse);
    bool is_applicable(ZXGraph const& graph) const override;
    bool is_undoable(ZXGraph const& graph) const override;
    void apply_unchecked(ZXGraph& graph) const override;
    void undo_unchecked(ZXGraph& graph) const override;

private:
    NeighborUnfusion _nu;
    LComp _lcomp;

    bool _no_need_to_unfuse(ZXGraph const& graph) const;
    bool _no_need_to_undo_unfuse() const;
};

class PivotUnfusion : public ZXRule {
public:
    PivotUnfusion(size_t v1_id, size_t v2_id,
                  std::vector<size_t> const& neighbors_to_unfuse_v1,
                  std::vector<size_t> const& neighbors_to_unfuse_v2);

    bool is_applicable(ZXGraph const& graph) const override;
    bool is_undoable(ZXGraph const& graph) const override;
    void apply_unchecked(ZXGraph& graph) const override;
    void undo_unchecked(ZXGraph& graph) const override;

private:
    NeighborUnfusion _nu1;
    NeighborUnfusion _nu2;
    Pivot _pivot;

    bool _no_need_to_unfuse(ZXGraph const& graph,
                            NeighborUnfusion const& nu) const;
    bool _no_need_to_undo_unfuse(NeighborUnfusion const& nu) const;
};

}  // namespace qsyn::zx
