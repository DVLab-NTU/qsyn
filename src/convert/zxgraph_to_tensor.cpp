/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define class ZX-to-Tensor Mapper member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/
#include "./zxgraph_to_tensor.hpp"

#include <spdlog/spdlog.h>

#include <cassert>
#include <limits>
#include <ranges>
#include <tl/enumerate.hpp>
#include <tl/fold.hpp>
#include <tl/to.hpp>
#include <tl/zip.hpp>
#include <unordered_map>

#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"

extern bool stop_requested();

namespace qsyn {

namespace {

class ZX2TSMapper {
public:
    using Frontiers = dvlab::utils::ordered_hashmap<zx::EdgePair, size_t, zx::EdgePairHash>;

    std::optional<tensor::QTensor<double>> map(zx::ZXGraph const& graph);

private:
    std::vector<zx::EdgePair> _boundary_edges;  // EdgePairs of the boundaries

    struct FrontiersTensorPair {
        Frontiers frontiers;
        tensor::QTensor<double> tensor;
    };
    std::vector<FrontiersTensorPair> _zx2ts_list;  // The tensor list for each set of frontiers
    size_t _current_tensor_id = 0;                 // Current tensor id for the _tensorId

    std::unordered_map<zx::ZXVertex*, size_t> _pins;

    struct MappingInfo {
        qsyn::tensor::TensorAxisList simple_edge_pins;    // Axes that can be tensordotted directly
        qsyn::tensor::TensorAxisList hadamard_edge_pins;  // Axes that should be applied hadamards first
        std::vector<zx::EdgePair> frontiers_to_remove;    // Old frontiers to be removed
        std::vector<zx::EdgePair> frontiers_to_add;       // New frontiers to be added
    };

    auto& _frontiers(size_t id) { return _zx2ts_list.at(id).frontiers; }
    auto& _tensor(size_t id) { return _zx2ts_list.at(id).tensor; }
    auto const& _frontiers(size_t id) const { return _zx2ts_list.at(id).frontiers; }
    auto const& _tensor(size_t id) const { return _zx2ts_list.at(id).tensor; }

    auto& _curr_frontiers() { return _frontiers(_current_tensor_id); }
    auto& _curr_tensor() { return _tensor(_current_tensor_id); }
    auto const& _curr_frontiers() const { return _frontiers(_current_tensor_id); }
    auto const& _curr_tensor() const { return _tensor(_current_tensor_id); }

    void _map_one_vertex(zx::ZXGraph const& graph, zx::ZXVertex* v);

    // mapOneVertex Subroutines
    void _initialize_subgraph(zx::ZXGraph const& graph, zx::ZXVertex* v);
    MappingInfo _calculate_mapping_info(zx::ZXGraph const& graph, zx::ZXVertex* v);
    tensor::QTensor<double> _dehadamardize(tensor::QTensor<double> const& ts, MappingInfo& info);
    void _tensordot_vertex(zx::ZXGraph const& graph, zx::ZXVertex* v);

    size_t _get_tensor_id(zx::ZXGraph const& graph, zx::ZXVertex* v);
    bool _is_frontier(zx::NeighborPair const& nbr) const { return _pins.contains(nbr.first); }

    struct InOutAxisList {
        qsyn::tensor::TensorAxisList inputs;
        qsyn::tensor::TensorAxisList outputs;

        InOutAxisList(size_t num_inputs, size_t num_outputs) : inputs(num_inputs), outputs(num_outputs) {}
    };
    InOutAxisList _get_axis_orders(zx::ZXGraph const& zxgraph);
};

}  // namespace

std::optional<tensor::QTensor<double>> to_tensor(zx::ZXGraph const& zxgraph) {
    ZX2TSMapper mapper;
    return mapper.map(zxgraph);
}

/**
 * @brief convert a zxgraph to a tensor
 *
 * @return std::optional<QTensor<double>> containing a QTensor<double> if the conversion succeeds
 */
std::optional<tensor::QTensor<double>> ZX2TSMapper::map(zx::ZXGraph const& graph) try {
    using namespace std::complex_literals;
    if (graph.is_empty()) {
        spdlog::error("The ZXGraph is empty!!");
        return std::nullopt;
    }
    if (!graph.is_valid()) {
        spdlog::error("The ZXGraph is not valid!!");
        return std::nullopt;
    }

    graph.topological_traverse([&graph, this](zx::ZXVertex* v) { _map_one_vertex(graph, v); });

    if (stop_requested()) {
        spdlog::error("Conversion is interrupted!!");
        return std::nullopt;
    }

    tensor::QTensor<double> result =
        tl::fold_left(_zx2ts_list, tensor::QTensor<double>(1. + 0.i),
                      [](auto const& a, auto const& b) { return tensordot(a, b.tensor); });

    std::ranges::for_each(std::views::iota(0ul, _zx2ts_list.size()), [this](auto const i) {
        // We don't care whether key collision happen because _get_axis_orders takes care of such cases
        _frontiers(i).emplace(_boundary_edges[i], 0);
    });

    auto const [input_ids, output_ids] = _get_axis_orders(graph);

    spdlog::trace("Input  Axis IDs: {}", fmt::join(input_ids, " "));
    spdlog::trace("Output Axis IDs: {}", fmt::join(output_ids, " "));
    result = result.to_matrix(output_ids, input_ids);

    return result;
} catch (std::bad_alloc& e) {
    spdlog::error("Memory allocation failed!!");
    return std::nullopt;
}

/**
 * @brief Get Tensor form of Z, X spider, or H box
 *
 * @param v the ZXVertex
 * @return QTensor<double>
 */
tensor::QTensor<double> get_tensor_form(zx::ZXGraph const& graph, zx::ZXVertex* v) {
    using namespace std::complex_literals;
    switch (v->get_type()) {
        case zx::VertexType::z:
            return tensor::QTensor<double>::zspider(graph.get_num_neighbors(v), v->get_phase());
        case zx::VertexType::x:
            return tensor::QTensor<double>::xspider(graph.get_num_neighbors(v), v->get_phase());
        case zx::VertexType::h_box:
            return tensor::QTensor<double>::hbox(graph.get_num_neighbors(v));
        case zx::VertexType::boundary:
            return tensor::QTensor<double>::identity(graph.get_num_neighbors(v));
        default:
            spdlog::critical("Invalid vertex type!! ({})", v->get_id());
            exit(1);
    }
}

namespace {

/**
 * @brief Construct tensor of a single vertex
 *
 * @param v the tensor of whom
 */
void ZX2TSMapper::_map_one_vertex(zx::ZXGraph const& graph, zx::ZXVertex* v) {
    if (stop_requested()) return;
    _current_tensor_id = _get_tensor_id(graph, v);

    if (_current_tensor_id == _pins.size() /* is a new subgraph */) {
        _initialize_subgraph(graph, v);
    } else {
        _tensordot_vertex(graph, v);
    }
    _pins.emplace(v, _current_tensor_id);

    spdlog::debug("Done. Current tensor dimension: {}", _curr_tensor().dimension());
    spdlog::trace("Current frontiers:");
    for (auto& [epair, axid] : _curr_frontiers()) {
        auto& [vpair, etype] = epair;
        spdlog::trace("  {}--{} ({}) axis id: {}", vpair.first->get_id(), vpair.second->get_id(), etype, axid);
    }
}

/**
 * @brief Generate a new subgraph for mapping
 *
 * @param v the boundary vertex to start the mapping
 */
void ZX2TSMapper::_initialize_subgraph(zx::ZXGraph const& graph, zx::ZXVertex* v) {
    using namespace std::complex_literals;
    assert(v->is_boundary());
    spdlog::debug("Mapping vertex {:>4} ({}): New Subgraph", v->get_id(), v->get_type());
    auto [nb, etype] = graph.get_first_neighbor(v);

    _current_tensor_id = _zx2ts_list.size();
    _zx2ts_list.emplace_back(Frontiers(), tensor::QTensor<double>(1. + 0.i));

    _curr_tensor()      = tensordot(_curr_tensor(), tensor::QTensor<double>::identity(graph.get_num_neighbors(v)));
    auto const edge_key = make_edge_pair(v, nb, etype);
    _boundary_edges.emplace_back(edge_key);
    _curr_frontiers().emplace(edge_key, 1);
}

/**
 * @brief Check which tensor an untraversed vertex belongs to
 *
 * @param v vertex
 * @return the tensor id
 */
size_t ZX2TSMapper::_get_tensor_id(zx::ZXGraph const& graph, zx::ZXVertex* v) {
    auto const it = std::ranges::find_if(
        graph.get_neighbors(v),
        [this](auto const& neighbor) {
            return _is_frontier(neighbor);
        });

    if (it != graph.get_neighbors(v).end()) {
        return _pins.at(it->first);
    }

    return _pins.size();
}

/**
 * @brief get the tensor axis-zxgraph qubit correspondence
 *
 * @param zxgraph
 * @return std::pair<TensorAxisList, TensorAxisList> input and output tensor axis lists
 */
ZX2TSMapper::InOutAxisList ZX2TSMapper::_get_axis_orders(zx::ZXGraph const& zxgraph) {
    InOutAxisList axis_lists{zxgraph.get_num_inputs(), zxgraph.get_num_outputs()};

    auto const get_table = [](auto vertex_list) -> std::map<int, size_t> {
        vertex_list.sort([](auto const& a, auto const& b) { return a->get_qubit() < b->get_qubit(); });
        std::map<int, size_t> table;
        for (auto&& [i, v] : tl::views::enumerate(vertex_list)) {
            table[v->get_qubit()] = i;
        }
        return table;
    };
    auto const input_table  = get_table(zxgraph.get_inputs());
    auto const output_table = get_table(zxgraph.get_outputs());

    size_t acc_frontier_size = 0;
    for (size_t i = 0; i < _zx2ts_list.size(); ++i) {
        bool has_boundary_to_boundary_edge = false;
        for (auto& [epair, axid] : _frontiers(i)) {
            auto const& [v1, v2]    = epair.first;
            auto const v1_is_input  = zxgraph.get_inputs().contains(v1);
            auto const v2_is_input  = zxgraph.get_inputs().contains(v2);
            auto const v1_is_output = zxgraph.get_outputs().contains(v1);
            auto const v2_is_output = zxgraph.get_outputs().contains(v2);

            if (v1_is_input) axis_lists.inputs[input_table.at(v1->get_qubit())] = axid + acc_frontier_size;
            if (v2_is_input) axis_lists.inputs[input_table.at(v2->get_qubit())] = axid + acc_frontier_size;
            if (v1_is_output) axis_lists.outputs[output_table.at(v1->get_qubit())] = axid + acc_frontier_size;
            if (v2_is_output) axis_lists.outputs[output_table.at(v2->get_qubit())] = axid + acc_frontier_size;
            assert(!(v1_is_input && v1_is_output));
            assert(!(v2_is_input && v2_is_output));

            // If seeing boundary-to-boundary edge, increase one of the axis id by one to avoid id collision
            if (v1_is_input && (v2_is_input || v2_is_output)) {
                assert(_frontiers(i).size() == 1);
                axis_lists.inputs[input_table.at(v1->get_qubit())]--;
                has_boundary_to_boundary_edge = true;
            }
            if (v1_is_output && (v2_is_input || v2_is_output)) {
                assert(_frontiers(i).size() == 1);
                axis_lists.outputs[output_table.at(v1->get_qubit())]--;
                has_boundary_to_boundary_edge = true;
            }
        }
        acc_frontier_size += _frontiers(i).size() + (has_boundary_to_boundary_edge ? 1 : 0);
    }

    return axis_lists;
}

/**
 * @brief Update information for the current and next frontiers
 *
 * @param v the current vertex
 */
ZX2TSMapper::MappingInfo ZX2TSMapper::_calculate_mapping_info(zx::ZXGraph const& graph, zx::ZXVertex* v) {
    MappingInfo info;

    for (auto& nbr : graph.get_neighbors(v)) {
        auto& [nb, etype] = nbr;

        auto edge_key = make_edge_pair(v, nb, etype);
        if (!_is_frontier(nbr)) {
            info.frontiers_to_add.emplace_back(edge_key);
        } else {
            auto& [frontier, axid] = *(_curr_frontiers().find(edge_key));
            if ((frontier.second) == zx::EdgeType::hadamard) {
                info.hadamard_edge_pins.emplace_back(axid);
            } else {
                info.simple_edge_pins.emplace_back(axid);
            }
            info.frontiers_to_remove.emplace_back(edge_key);
        }
    }

    return info;
}

/**
 * @brief Convert hadamard edges to normal edges and returns a corresponding tensor
 *
 * @param ts original tensor before converting
 * @return QTensor<double>
 */
tensor::QTensor<double> ZX2TSMapper::_dehadamardize(tensor::QTensor<double> const& ts, MappingInfo& info) {
    auto const h_tensor_product = tensor_product_pow(
        tensor::QTensor<double>::hbox(2), info.hadamard_edge_pins.size());

    qsyn::tensor::TensorAxisList connect_pin =
        std::views::iota(0ul, info.hadamard_edge_pins.size()) |
        std::views::transform([](auto i) { return 2 * i; }) |
        tl::to<std::vector>();

    auto tmp = tensordot(ts, h_tensor_product, info.hadamard_edge_pins, connect_pin);

    // post-tensordot axis update
    for (auto& [_, axis_id] : _curr_frontiers()) {
        auto const it = std::ranges::find(info.hadamard_edge_pins, axis_id);
        if (it != info.hadamard_edge_pins.end()) {
            auto const id = it - info.hadamard_edge_pins.begin();
            axis_id       = tmp.get_new_axis_id(ts.dimension() + connect_pin[id] + 1);
        } else {
            axis_id = tmp.get_new_axis_id(axis_id);
        }
    }

    // update edge pins
    for (auto&& [h_pin, c_pin] : tl::views::zip(info.hadamard_edge_pins, connect_pin)) {
        h_pin = tmp.get_new_axis_id(ts.dimension() + c_pin + 1);
    }

    for (auto& pin : info.simple_edge_pins) {
        pin = tmp.get_new_axis_id(pin);
    }

    info.simple_edge_pins = tensor::concat_axis_list(info.hadamard_edge_pins, info.simple_edge_pins);
    return tmp;
}

/**
 * @brief Tensordot the current tensor to the tensor of vertex v
 *
 * @param v current vertex
 */
void ZX2TSMapper::_tensordot_vertex(zx::ZXGraph const& graph, zx::ZXVertex* v) {
    auto info = _calculate_mapping_info(graph, v);

    if (v->is_boundary()) {
        spdlog::debug("Mapping vertex {:>4} ({}): Boundary", v->get_id(), v->get_type());
        _curr_tensor() = _dehadamardize(_curr_tensor(), info);
        return;
    }

    spdlog::debug("Mapping vertex {:>4} ({}): Tensordot", v->get_id(), v->get_type());
    auto const dehadamarded = _dehadamardize(_curr_tensor(), info);
    // we don't care which pins to connect because all vertices correspond to a symmetric tensor
    auto const vertex_pins_to_connect = std::views::iota(0ul, info.simple_edge_pins.size()) | tl::to<std::vector>();

    _curr_tensor() = tensordot(dehadamarded, get_tensor_form(graph, v), info.simple_edge_pins, vertex_pins_to_connect);

    for (auto const& edge : info.frontiers_to_remove) {
        _curr_frontiers().erase(edge);
    }

    // post-tensordot axis id update
    for (auto& ax_id : _curr_frontiers() | std::views::values) {
        ax_id = _curr_tensor().get_new_axis_id(ax_id);
    }

    // add new frontiers
    for (auto&& [t, edge] : tl::views::enumerate(info.frontiers_to_add)) {
        auto const new_id = _curr_tensor().get_new_axis_id(dehadamarded.dimension() + vertex_pins_to_connect.size() + t);
        _curr_frontiers().emplace(edge, new_id);
    }
}

}  // namespace

}  // namespace qsyn
