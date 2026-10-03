#pragma once

#include <fmt/core.h>
#include <fmt/format.h>

#include <cstddef>
#include <ranges>
#include <type_traits>
#include <unordered_map>

#include "util/ordered_hashmap.hpp"
#include "util/ordered_hashset.hpp"

namespace dvlab {

template <typename VertexAttr = void, typename EdgeAttr = void>
class Digraph {
    constexpr static bool is_vertex_attr_default_constructible =
        std::is_default_constructible_v<VertexAttr>;
    constexpr static bool is_edge_attr_default_constructible =
        std::is_default_constructible_v<EdgeAttr>;
    constexpr static bool has_vertex_attr =
        !std::is_same_v<VertexAttr, void>;
    constexpr static bool has_edge_attr =
        !std::is_same_v<EdgeAttr, void>;

public:
    using Vertex = size_t;
    struct Edge {
        Vertex src, dst;
        bool operator==(Edge const& other) const = default;
    };
    using NeighborSet = dvlab::utils::ordered_hashset<Vertex>;

    Digraph() = default;
    Digraph(size_t num_vertices) {
        for (size_t i = 0; i < num_vertices; ++i) {
            add_vertex();
        }
    }

    std::enable_if_t<is_vertex_attr_default_constructible ||
                         !has_vertex_attr,
                     Vertex>
    add_vertex() {
        Vertex const v = _next_vertex_id++;
        if constexpr (has_vertex_attr) {
            _vertex_attributes[v] = VertexAttr{};
        } else {
            _vertex_attributes.insert(v);
        }
        _in_neighbors[v]  = {};
        _out_neighbors[v] = {};
        return v;
    }

    std::enable_if_t<has_vertex_attr, Vertex>
    add_vertex(VertexAttr const& attr) {
        Vertex const v        = _next_vertex_id++;
        _vertex_attributes[v] = attr;
        _in_neighbors[v]      = {};
        _out_neighbors[v]     = {};
        return v;
    }

    std::enable_if_t<is_vertex_attr_default_constructible ||
                         !has_vertex_attr,
                     bool>
    add_vertex_with_id(Vertex v) {
        if (_vertex_attributes.contains(v)) return false;
        if constexpr (has_vertex_attr) {
            _vertex_attributes[v] = VertexAttr{};
        } else {
            _vertex_attributes.insert(v);
        }
        _in_neighbors[v]  = {};
        _out_neighbors[v] = {};
        if (v >= _next_vertex_id) {
            _next_vertex_id = v + 1;
        }
        return true;
    }

    std::enable_if_t<has_vertex_attr, bool>
    add_vertex_with_id(
        Vertex v,
        VertexAttr const& attr) {
        if (_vertex_attributes.contains(v)) return false;
        _vertex_attributes[v] = attr;
        _in_neighbors[v]      = {};
        _out_neighbors[v]     = {};
        if (v >= _next_vertex_id) {
            _next_vertex_id = v + 1;
        }
        return true;
    }

    size_t remove_vertex(Vertex v) {
        auto n = _vertex_attributes.erase(v);
        // remove all edges connected to v
        auto out_it = _out_neighbors.find(v);
        if (out_it != _out_neighbors.end()) {
            for (auto dst : out_it->second) {
                auto in_it = _in_neighbors.find(dst);
                if (in_it != _in_neighbors.end()) {
                    in_it->second.erase(v);
                }
                if constexpr (has_edge_attr) {
                    _edge_attributes.erase(Edge{v, dst});
                } else {
                    --_edge_attributes;
                }
            }
            _out_neighbors.erase(out_it);
        }

        auto in_it = _in_neighbors.find(v);
        if (in_it != _in_neighbors.end()) {
            for (auto src : in_it->second) {
                auto out_it2 = _out_neighbors.find(src);
                if (out_it2 != _out_neighbors.end()) {
                    out_it2->second.erase(v);
                }
                if constexpr (has_edge_attr) {
                    _edge_attributes.erase(Edge{src, v});
                } else {
                    --_edge_attributes;
                }
            }
            _in_neighbors.erase(in_it);
        }

        return n;
    }

    std::enable_if_t<is_edge_attr_default_constructible || !has_edge_attr, Edge>
    add_edge(Vertex src, Vertex dst) {
        _out_neighbors[src].insert(dst);
        _in_neighbors[dst].insert(src);

        if constexpr (has_edge_attr) {
            Edge e              = {src, dst};
            _edge_attributes[e] = EdgeAttr{};
            return e;
        } else {
            ++_edge_attributes;
            return Edge{src, dst};
        }
    }

    std::enable_if_t<is_edge_attr_default_constructible || !has_edge_attr, Edge>
    add_edge(Edge e) {
        return add_edge(e.src, e.dst);
    }

    // add_edge(src, dst, attr) with attribute
    template <typename EA = EdgeAttr>
    auto add_edge(Vertex src, Vertex dst, EA const& attr)
        -> std::enable_if_t<has_edge_attr && !std::is_same_v<EA, void>, Edge> {
        Edge e = {src, dst};
        _out_neighbors[src].insert(dst);
        _in_neighbors[dst].insert(src);
        _edge_attributes[e] = attr;
        return e;
    }

    // add_edge(Edge e, attr) with attribute
    template <typename EA = EdgeAttr>
    auto add_edge(Edge e, EA const& attr)
        -> std::enable_if_t<has_edge_attr && !std::is_same_v<EA, void>, Edge> {
        return add_edge(e.src, e.dst, attr);
    }

    size_t remove_edge(Vertex src, Vertex dst) {
        if (!has_edge(src, dst)) return 0;
        _out_neighbors.at(src).erase(dst);
        _in_neighbors.at(dst).erase(src);
        if constexpr (has_edge_attr) {
            _edge_attributes.erase(Edge{src, dst});
        } else {
            --_edge_attributes;
        }
        return 1;
    }

    size_t remove_edge(Edge e) {
        return remove_edge(e.src, e.dst);
    }

    bool has_edge(Vertex src, Vertex dst) const {
        return has_vertex(src) && has_vertex(dst) &&
               _out_neighbors.at(src).contains(dst);
    }

    bool has_edge(Edge e) const { return has_edge(e.src, e.dst); }

    auto vertices() const {
        if constexpr (has_vertex_attr) {
            return _vertex_attributes | std::views::keys;
        } else {
            return std::ranges::subrange(
                _vertex_attributes.begin(),
                _vertex_attributes.end());
        }
    }

    auto in_edges(Vertex v) const {
        return _in_neighbors.at(v) |
               std::views::transform(
                   [v](Vertex src) { return Edge{src, v}; });
    }

    auto out_edges(Vertex v) const {
        return _out_neighbors.at(v) |
               std::views::transform(
                   [v](Vertex dst) { return Edge{v, dst}; });
    }

    auto edges() const {
        auto transformed_range =
            vertices() |
            std::views::transform(
                [this](auto const& v) {
                    return out_edges(v);
                });

        auto joined_range = std::views::join(transformed_range);

        return joined_range;
    }

    auto const& in_neighbors(Vertex v) const {
        return _in_neighbors.at(v);
    }
    auto const& out_neighbors(Vertex v) const {
        return _out_neighbors.at(v);
    }

    size_t out_degree(Vertex v) const { return _out_neighbors.at(v).size(); }
    size_t in_degree(Vertex v) const { return _in_neighbors.at(v).size(); }
    size_t degree(Vertex v) const { return out_degree(v) + in_degree(v); }
    size_t num_vertices() const { return _vertex_attributes.size(); }
    size_t num_edges() const {
        if constexpr (has_edge_attr) {
            return _edge_attributes.size();
        } else {
            return _edge_attributes;
        }
    }

    bool has_vertex(Vertex v) const {
        return _vertex_attributes.contains(v);
    }

    std::enable_if_t<has_vertex_attr, VertexAttr const&>
    vertex_attr(Vertex v) const {
        return _vertex_attributes.at(v);
    }

    std::enable_if_t<has_vertex_attr, VertexAttr&>
    vertex_attr(Vertex v) {
        return _vertex_attributes.at(v);
    }

    std::enable_if_t<has_vertex_attr, VertexAttr const&>
    operator[](Vertex v) const {
        return vertex_attr(v);
    }

    std::enable_if_t<has_vertex_attr, VertexAttr&>
    operator[](Vertex v) {
        return vertex_attr(v);
    }

    template <typename T = EdgeAttr>
    requires(has_edge_attr && !std::is_void_v<T>)
    T const& edge_attr(Edge e) const {
        return _edge_attributes.at(e);
    }

    template <typename T = EdgeAttr>
    requires(has_edge_attr && !std::is_void_v<T>)
    T& edge_attr(Edge e) {
        return _edge_attributes.at(e);
    }

    template <typename T = EdgeAttr>
    requires(has_edge_attr && !std::is_void_v<T>)
    T const& operator[](Edge e) const {
        return edge_attr(e);
    }

    template <typename T = EdgeAttr>
    requires(has_edge_attr && !std::is_void_v<T>)
    T& operator[](Edge e) {
        return edge_attr(e);
    }

    bool operator==(Digraph const& other) const {
        if (num_vertices() != other.num_vertices()) return false;
        if (num_edges() != other.num_edges()) return false;

        for (auto v : vertices()) {
            if (!other.has_vertex(v)) return false;
            if constexpr (has_vertex_attr) {
                if (vertex_attr(v) != other.vertex_attr(v)) return false;
            }
        }

        for (auto const& v : vertices()) {
            for (auto const& e : out_edges(v)) {
                if (!other.has_edge(e)) return false;
                if constexpr (has_edge_attr) {
                    if (edge_attr(e) != other.edge_attr(e)) return false;
                }
            }
        }
        return true;
    }

    bool operator!=(Digraph const& other) const {
        return !(*this == other);
    }

private:
    Vertex _next_vertex_id = 0;

    std::unordered_map<Vertex, NeighborSet> _out_neighbors;
    std::unordered_map<Vertex, NeighborSet> _in_neighbors;

    using VertexAttributes = std::conditional_t<
        has_vertex_attr,
        dvlab::utils::ordered_hashmap<Vertex, VertexAttr>,
        dvlab::utils::ordered_hashset<Vertex> >;

    VertexAttributes _vertex_attributes;

    struct EdgeHash {
        std::size_t operator()(Edge const& edge) const noexcept {
            std::size_t h1 = std::hash<Vertex>{}(edge.src);
            std::size_t h2 = std::hash<Vertex>{}(edge.dst);
            h1 ^= h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2);
            return h1;
        }
    };

    using EdgeAttributes = std::conditional_t<
        has_edge_attr,
        std::unordered_map<Edge, EdgeAttr, EdgeHash>,
        size_t>;

    EdgeAttributes _edge_attributes;
};

}  // namespace dvlab
