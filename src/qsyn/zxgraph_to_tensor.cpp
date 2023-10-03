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

#include "zx/zxgraph.hpp"

extern bool stop_requested();

namespace qsyn {

class ZX2TSMapper {
public:
    using Frontiers = dvlab::utils::ordered_hashmap<zx::EdgePair, size_t, zx::EdgePairHash>;

    ZX2TSMapper() {}

    class ZX2TSList {
    public:
        Frontiers const& frontiers(size_t const& id) const {
            return _zx2ts_list[id].first;
        }
        tensor::QTensor<double> const& tensor(size_t const& id) const {
            return _zx2ts_list[id].second;
        }
        Frontiers& frontiers(size_t const& id) {
            return _zx2ts_list[id].first;
        }
        tensor::QTensor<double>& tensor(size_t const& id) {
            return _zx2ts_list[id].second;
        }
        void append(Frontiers const& f, tensor::QTensor<double> const& q) {
            _zx2ts_list.emplace_back(f, q);
        }
        size_t size() {
            return _zx2ts_list.size();
        }

    private:
        std::vector<std::pair<Frontiers, tensor::QTensor<double>>> _zx2ts_list;
    };

    std::optional<tensor::QTensor<double>> map(zx::ZXGraph const& zxgraph);

private:
    std::vector<zx::EdgePair> _boundary_edges;  // EdgePairs of the boundaries
    ZX2TSList _zx2ts_list;                      // The tensor list for each set of frontiers
    size_t _tensor_id = 0;                      // Current tensor id for the _tensorId

    qsyn::tensor::TensorAxisList _simple_pins;    // Axes that can be tensordotted directly
    qsyn::tensor::TensorAxisList _hadamard_pins;  // Axes that should be applied hadamards first
    std::vector<zx::EdgePair> _remove_edges;      // Old frontiers to be removed
    std::vector<zx::EdgePair> _add_edges;         // New frontiers to be added

    Frontiers& _curr_frontiers() { return _zx2ts_list.frontiers(_tensor_id); }
    tensor::QTensor<double>& _curr_tensor() { return _zx2ts_list.tensor(_tensor_id); }
    Frontiers const& _curr_frontiers() const { return _zx2ts_list.frontiers(_tensor_id); }
    tensor::QTensor<double> const& _curr_tensor() const { return _zx2ts_list.tensor(_tensor_id); }

    void _map_one_vertex(zx::ZXVertex* v);

    // mapOneVertex Subroutines
    void _initialize_subgraph(zx::ZXVertex* v);
    void _tensordot_vertex(zx::ZXVertex* v);
    void _update_pins_and_frontiers(zx::ZXVertex* v);
    tensor::QTensor<double> _dehadamardize(tensor::QTensor<double> const& ts);

    bool _is_of_new_graph(zx::ZXVertex const* v);
    bool _is_frontier(zx::NeighborPair const& nbr) const;

    struct InOutAxisList {
        qsyn::tensor::TensorAxisList inputs;
        qsyn::tensor::TensorAxisList outputs;
    };
    InOutAxisList _get_axis_orders(zx::ZXGraph const& zxgraph);
};

std::optional<tensor::QTensor<double>> to_tensor(zx::ZXGraph const& zxgraph) {
    ZX2TSMapper mapper;
    return mapper.map(zxgraph);
}

/**
 * @brief convert a zxgraph to a tensor
 *
 * @return std::optional<QTensor<double>> containing a QTensor<double> if the conversion succeeds
 */
std::optional<tensor::QTensor<double>> ZX2TSMapper::map(zx::ZXGraph const& zxgraph) {
    if (!zxgraph.is_valid()) {
        spdlog::error("The ZXGraph is not valid!!");
        return std::nullopt;
    }

    for (auto& v : zxgraph.get_vertices()) {
        v->set_pin(unsigned(-1));
    }

    zxgraph.topological_traverse([this](zx::ZXVertex* v) { _map_one_vertex(v); });

    if (stop_requested()) {
        spdlog::error("Conversion is interrupted!!");
        return std::nullopt;
    }
    tensor::QTensor<double> result;

    for (size_t i = 0; i < _zx2ts_list.size(); ++i) {
        result = tensordot(result, _zx2ts_list.tensor(i));
    }

    for (size_t i = 0; i < _boundary_edges.size(); ++i) {
        // Don't care whether key collision happen: getAxisOrders takes care of such cases
        _zx2ts_list.frontiers(i).emplace(_boundary_edges[i], 0);
    }

    auto [inputIds, outputIds] = _get_axis_orders(zxgraph);

    spdlog::trace("Input  Axis IDs: {}", fmt::join(inputIds, " "));
    spdlog::trace("Output Axis IDs: {}", fmt::join(outputIds, " "));

    result = result.to_matrix(inputIds, outputIds);

    return result;
}

/**
 * @brief Consturct tensor of a single vertex
 *
 * @param v the tensor of whom
 */
void ZX2TSMapper::_map_one_vertex(zx::ZXVertex* v) {
    if (stop_requested()) return;

    _simple_pins.clear();
    _hadamard_pins.clear();
    _remove_edges.clear();
    _add_edges.clear();
    _tensor_id = 0;

    bool is_new_graph = _is_of_new_graph(v);
    bool is_boundary  = v->is_boundary();

    spdlog::debug("Mapping vertex {:>4} ({}): {}", v->get_id(), v->get_type(), is_new_graph ? "New Subgraph" : is_boundary ? "Boundary"
                                                                                                                           : "Tensordot");

    if (is_new_graph) {
        _initialize_subgraph(v);
    } else if (is_boundary) {
        _update_pins_and_frontiers(v);
        _curr_tensor() = _dehadamardize(_curr_tensor());
    } else {
        _update_pins_and_frontiers(v);
        _tensordot_vertex(v);
    }
    v->set_pin(_tensor_id);

    spdlog::debug("Done. Current tensor dimension: {}", _curr_tensor().dimension());
    spdlog::trace("Current frontiers:");
    for (auto& [epair, axid] : _zx2ts_list.frontiers(_tensor_id)) {
        auto& [vpair, etype] = epair;
        spdlog::trace("  {}--{} ({}) axis id: {}", vpair.first->get_id(), vpair.second->get_id(), etype, axid);
    }
}

/**
 * @brief Generate a new subgraph for mapping
 *
 * @param v the boundary vertex to start the mapping
 */
void ZX2TSMapper::_initialize_subgraph(zx::ZXVertex* v) {
    using namespace std::complex_literals;
    auto [nb, etype] = *(v->get_neighbors().begin());

    _zx2ts_list.append(Frontiers(), tensor::QTensor<double>(1. + 0.i));
    _tensor_id = _zx2ts_list.size() - 1;
    assert(v->is_boundary());

    zx::EdgePair edge_key = make_edge_pair(v, nb, etype);
    _curr_tensor()        = tensordot(_curr_tensor(), tensor::QTensor<double>::identity(v->get_num_neighbors()));
    _boundary_edges.emplace_back(edge_key);
    _curr_frontiers().emplace(edge_key, 1);
}

/**
 * @brief Check if a vertex belongs to a new subgraph that is not traversed
 *
 * @param v vertex
 * @return true or
 * @return false and set the _tensorId to the current tensor
 */
bool ZX2TSMapper::_is_of_new_graph(zx::ZXVertex const* v) {
    for (auto nbr : v->get_neighbors()) {
        if (_is_frontier(nbr)) {
            _tensor_id = nbr.first->get_pin();
            return false;
        }
    }
    return true;
}

/**
 * @brief get the tensor axis-zxgraph qubit correspondence
 *
 * @param zxgraph
 * @return std::pair<TensorAxisList, TensorAxisList> input and output tensor axis lists
 */
ZX2TSMapper::InOutAxisList ZX2TSMapper::_get_axis_orders(zx::ZXGraph const& zxgraph) {
    InOutAxisList axis_lists;
    axis_lists.inputs.resize(zxgraph.get_num_inputs());
    axis_lists.outputs.resize(zxgraph.get_num_outputs());
    std::map<int, size_t> input_table, output_table;  // std:: to avoid name collision with ZX2TSMapper::map
    for (auto v : zxgraph.get_inputs()) {
        input_table[v->get_qubit()] = 0;
    }
    size_t count = 0;
    for (auto [qid, _] : input_table) {
        input_table[qid] = count;
        ++count;
    }

    for (auto v : zxgraph.get_outputs()) {
        output_table[v->get_qubit()] = 0;
    }
    count = 0;
    for (auto [qid, _] : output_table) {
        output_table[qid] = count;
        ++count;
    }
    size_t acc_frontier_size = 0;
    for (size_t i = 0; i < _zx2ts_list.size(); ++i) {
        bool has_boundary2_boundary_edge = false;
        for (auto& [epair, axid] : _zx2ts_list.frontiers(i)) {
            auto const& [v1, v2] = epair.first;
            bool v1_is_input     = zxgraph.get_inputs().contains(v1);
            bool v2_is_input     = zxgraph.get_inputs().contains(v2);
            bool v1_is_output    = zxgraph.get_outputs().contains(v1);
            bool v2_is_output    = zxgraph.get_outputs().contains(v2);

            if (v1_is_input) axis_lists.inputs[input_table[v1->get_qubit()]] = axid + acc_frontier_size;
            if (v2_is_input) axis_lists.inputs[input_table[v2->get_qubit()]] = axid + acc_frontier_size;
            if (v1_is_output) axis_lists.outputs[output_table[v1->get_qubit()]] = axid + acc_frontier_size;
            if (v2_is_output) axis_lists.outputs[output_table[v2->get_qubit()]] = axid + acc_frontier_size;
            assert(!(v1_is_input && v1_is_output));
            assert(!(v2_is_input && v2_is_output));

            // If seeing boundary-to-boundary edge, decrease one of the axis id by one to avoid id collision
            if (v1_is_input && (v2_is_input || v2_is_output)) {
                assert(_zx2ts_list.frontiers(i).size() == 1);
                axis_lists.inputs[input_table[v1->get_qubit()]]--;
                has_boundary2_boundary_edge = true;
            }
            if (v1_is_output && (v2_is_input || v2_is_output)) {
                assert(_zx2ts_list.frontiers(i).size() == 1);
                axis_lists.outputs[output_table[v1->get_qubit()]]--;
                has_boundary2_boundary_edge = true;
            }
        }
        acc_frontier_size += _zx2ts_list.frontiers(i).size() + (has_boundary2_boundary_edge ? 1 : 0);
    }

    return axis_lists;
}

/**
 * @brief Update information for the current and next frontiers
 *
 * @param v the current vertex
 */
void ZX2TSMapper::_update_pins_and_frontiers(zx::ZXVertex* v) {
    zx::Neighbors nbrs = v->get_neighbors();

    // unordered_set<NeighborPair> seenFrontiers; // only for look-up
    for (auto& nbr : nbrs) {
        auto& [nb, etype] = nbr;

        zx::EdgePair edge_key = make_edge_pair(v, nb, etype);
        if (!_is_frontier(nbr)) {
            _add_edges.emplace_back(edge_key);
        } else {
            auto& [front, axid] = *(_curr_frontiers().find(edge_key));
            if ((front.second) == zx::EdgeType::hadamard) {
                _hadamard_pins.emplace_back(axid);
            } else {
                _simple_pins.emplace_back(axid);
            }
            _remove_edges.emplace_back(edge_key);
        }
    }
}

/**
 * @brief Convert hadamard edges to normal edges and returns a corresponding tensor
 *
 * @param ts original tensor before converting
 * @return QTensor<double>
 */
tensor::QTensor<double> ZX2TSMapper::_dehadamardize(tensor::QTensor<double> const& ts) {
    tensor::QTensor<double> h_tensor_product = tensor_product_pow(
        tensor::QTensor<double>::hbox(2), _hadamard_pins.size());

    qsyn::tensor::TensorAxisList connect_pin;
    for (size_t t = 0; t < _hadamard_pins.size(); t++)
        connect_pin.emplace_back(2 * t);

    tensor::QTensor<double> tmp = tensordot(ts, h_tensor_product, _hadamard_pins, connect_pin);

    // post-tensordot axis update
    for (auto& [_, axisId] : _curr_frontiers()) {
        if (std::find(_hadamard_pins.begin(), _hadamard_pins.end(), axisId) == _hadamard_pins.end()) {
            axisId = tmp.get_new_axis_id(axisId);
        } else {
            size_t id = std::find(_hadamard_pins.begin(), _hadamard_pins.end(), axisId) - _hadamard_pins.begin();
            axisId    = tmp.get_new_axis_id(ts.dimension() + connect_pin[id] + 1);
        }
    }

    // update _simplePins and _hadamardPins
    for (size_t t = 0; t < _hadamard_pins.size(); t++) {
        _hadamard_pins[t] = tmp.get_new_axis_id(ts.dimension() + connect_pin[t] + 1);  // dimension of big tensor + 1,3,5,7,9
    }
    for (size_t t = 0; t < _simple_pins.size(); t++)
        _simple_pins[t] = tmp.get_new_axis_id(_simple_pins[t]);

    _simple_pins = qsyn::tensor::concat_axis_list(_hadamard_pins, _simple_pins);
    return tmp;
}

/**
 * @brief Tensordot the current tensor to the tensor of vertex v
 *
 * @param v current vertex
 */
void ZX2TSMapper::_tensordot_vertex(zx::ZXVertex* v) {
    tensor::QTensor<double> dehadamarded = _dehadamardize(_curr_tensor());

    tensor::TensorAxisList connect_pin;
    for (size_t t = 0; t < _simple_pins.size(); t++)
        connect_pin.emplace_back(t);

    _curr_tensor() = tensordot(dehadamarded, get_tensor_form(v), _simple_pins, connect_pin);

    // remove dotted frontiers
    for (size_t i = 0; i < _remove_edges.size(); i++)
        _curr_frontiers().erase(_remove_edges[i]);  // Erase old edges

    // post-tensordot axis id update
    for (auto& frontier : _curr_frontiers()) {
        frontier.second = _curr_tensor().get_new_axis_id(frontier.second);
    }

    // add new frontiers
    connect_pin.clear();
    for (size_t t = 0; t < _add_edges.size(); t++)
        connect_pin.emplace_back(_simple_pins.size() + t);

    for (size_t t = 0; t < _add_edges.size(); t++) {
        size_t new_id = _curr_tensor().get_new_axis_id(dehadamarded.dimension() + connect_pin[t]);
        _curr_frontiers().emplace(_add_edges[t], new_id);  // origin pin (neighbot count) + 1,3,5,7,9
    }
}

/**
 * @brief Check the neighbor pair (edge) is in frontier
 *
 * @param nbr
 * @return true
 * @return false
 */
bool ZX2TSMapper::_is_frontier(zx::NeighborPair const& nbr) const {
    return (nbr.first->get_pin() != std::numeric_limits<unsigned>::max());
}

/**
 * @brief Get Tensor form of Z, X spider, or H box
 *
 * @param v the ZXVertex
 * @return QTensor<double>
 */
tensor::QTensor<double> get_tensor_form(zx::ZXVertex* v) {
    using namespace std::complex_literals;
    if (v->is_boundary()) return tensor::QTensor<double>::identity(v->get_num_neighbors());
    if (v->is_hbox()) return tensor::QTensor<double>::hbox(v->get_num_neighbors());
    if (v->is_z()) return tensor::QTensor<double>::zspider(v->get_num_neighbors(), v->get_phase());
    if (v->is_x()) return tensor::QTensor<double>::xspider(v->get_num_neighbors(), v->get_phase());

    std::cerr << "Error: Invalid vertex type!! (" << v->get_id() << ")" << std::endl;
    return {1. + 0.i};
}

}  // namespace qsyn