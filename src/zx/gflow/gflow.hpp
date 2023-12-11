/****************************************************************************
  PackageName  [ gflow ]
  Synopsis     [ Define class GFlow structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#pragma once

#include <fmt/core.h>

#include <unordered_map>
#include <vector>

#include "../zxgraph.hpp"
#include "util/boolean_matrix.hpp"

namespace dvlab {

class BooleanMatrix;

}

namespace qsyn {

namespace zx {

class GFlow {
public:
    using Levels           = std::vector<ZXVertexList>;
    using CorrectionSetMap = std::unordered_map<ZXVertex*, ZXVertexList>;
    enum class MeasurementPlane {
        xy,
        yz,
        xz,
        not_a_qubit,
        error
    };
    using MeasurementPlaneMap = std::unordered_map<ZXVertex*, MeasurementPlane>;

    GFlow(ZXGraph* g) : _zxgraph{g} {}

    bool calculate();

    Levels const& get_levels() const { return _levels; }
    CorrectionSetMap const& get_x_correction_sets() const { return _x_correction_sets; }
    MeasurementPlaneMap const& get_measurement_planes() const { return _measurement_planes; }

    size_t get_level(ZXVertex* v) const { return _vertex2levels.at(v); }
    ZXVertexList const& get_x_correction_set(ZXVertex* v) const { return _x_correction_sets.at(v); }
    ZXVertexList get_z_correction_set(ZXVertex* v) const;
    MeasurementPlane const& get_measurement_plane(ZXVertex* v) const { return _measurement_planes.at(v); }

    bool is_z_error(ZXVertex* v) const { return !_do_extended ||
                                                _measurement_planes.at(v) == MeasurementPlane::xy ||
                                                _measurement_planes.at(v) == MeasurementPlane::xz; }
    bool is_x_error(ZXVertex* v) const { return _do_extended &&
                                                (_measurement_planes.at(v) == MeasurementPlane::xz ||
                                                 _measurement_planes.at(v) == MeasurementPlane::yz); }

    bool is_valid() const { return _valid; }

    void do_independent_layers(bool flag) { _do_independent_layers = flag; }
    void do_extended_gflow(bool flag) { _do_extended = flag; }

    void print() const;
    void print_levels() const;
    void print_x_correction_sets() const;
    void print_x_correction_set(ZXVertex* v) const;
    void print_summary() const;
    void print_failed_vertices() const;

private:
    ZXGraph* _zxgraph;
    Levels _levels;
    CorrectionSetMap _x_correction_sets;
    std::unordered_map<ZXVertex*, MeasurementPlane> _measurement_planes;
    std::unordered_map<ZXVertex*, size_t> _vertex2levels;

    bool _valid                 = false;
    bool _do_independent_layers = false;
    bool _do_extended           = false;

    // helper members
    ZXVertexList _frontier;
    ZXVertexList _neighbors;
    std::unordered_set<ZXVertex*> _taken;

    // gflow calculation subroutines
    void _initialize();
    void _calculate_zeroth_layer();
    void _update_neighbors_by_frontier();
    dvlab::BooleanMatrix _prepare_matrix(ZXVertex* v, size_t i, dvlab::BooleanMatrix const& matrix);
    void _set_correction_set_by_matrix(ZXVertex* v, dvlab::BooleanMatrix const& matrix);
    void _update_frontier();
};

}  // namespace zx

}  // namespace qsyn

std::ostream& operator<<(std::ostream& os, qsyn::zx::GFlow::MeasurementPlane const& plane);

template <>
struct fmt::formatter<qsyn::zx::GFlow::MeasurementPlane> {
    using MeasurementPlane = qsyn::zx::GFlow::MeasurementPlane;
    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
        return std::begin(ctx);
    }

    auto format(qsyn::zx::GFlow::MeasurementPlane const& plane, format_context& ctx) const -> format_context::iterator {
        switch (plane) {
            case MeasurementPlane::xy:
                return fmt::format_to(ctx.out(), "XY");
            case MeasurementPlane::yz:
                return fmt::format_to(ctx.out(), "YZ");
            case MeasurementPlane::xz:
                return fmt::format_to(ctx.out(), "XZ");
            case MeasurementPlane::not_a_qubit:
                return fmt::format_to(ctx.out(), "not a qubit");
            case MeasurementPlane::error:
            default:
                return fmt::format_to(ctx.out(), "ERROR");
        }
    }
};
