#pragma once

#include <cassert>
#include <cstddef>
#include <deque>
#include <vector>

namespace dvlab {

/**
 * @brief High-performance DAG peeler optimized for repeated layer extraction and vertex removal
 *
 * Uses vector-based adjacency lists with lazy deletion for maximum performance.
 * Maintain queues of degree-0 vertices for O(1) layer access
 */
class DagPeeler {
public:
    using Vertex                 = std::size_t;
    static constexpr Vertex npos = static_cast<Vertex>(-1);

    /**
     * @brief Construct a DagPeeler with n vertices
     * @param n Number of vertices (vertices are 0, 1, ..., n-1)
     */
    explicit DagPeeler(std::size_t n)
        : _n(n),
          out_adj(n),
          in_adj(n),
          in_deg(n, 0),
          out_deg(n, 0),
          alive(n, true),
          _alive_cnt(n) {}

    /**
     * @brief Add an edge from u to v (build phase)
     * Call this during graph construction, before finalize()
     */
    void add_edge(Vertex u, Vertex v) {
        assert(u < _n && v < _n);
        out_adj[u].push_back(v);
        ++out_deg[u];
        ++in_deg[v];
    }

    /**
     * @brief Finalize the graph after all edges are added
     * Builds in_adj and initializes queues
     */
    void finalize() {
        // Build in_adj in one scan
        for (Vertex u = 0; u < _n; ++u) {
            for (Vertex v : out_adj[u]) {
                in_adj[v].push_back(u);
            }
        }

        // Initialize queues with degree-0 vertices
        for (Vertex v = 0; v < _n; ++v) {
            if (!alive[v]) continue;
            if (in_deg[v] == 0) first_q.push_back(v);
            if (out_deg[v] == 0) last_q.push_back(v);
        }
    }

    /**
     * @brief Check if the graph is empty (all vertices removed)
     */
    bool empty() const { return _alive_cnt == 0; }

    /**
     * @brief Check if a vertex is still alive
     */
    bool is_alive(Vertex v) const {
        if (v >= _n) return false;
        return alive[v];
    }

    /**
     * @brief Get in-degree of a vertex
     */
    std::size_t in_degree(Vertex v) const {
        if (v >= _n) return 0;
        return in_deg[v];
    }

    /**
     * @brief Get out-degree of a vertex
     */
    std::size_t out_degree(Vertex v) const {
        if (v >= _n) return 0;
        return out_deg[v];
    }

    /**
     * @brief Get one vertex from first layer (in_deg == 0)
     * Uses lazy evaluation: skips dead vertices or vertices with non-zero degree
     * @return Vertex ID or npos if no vertex available
     */
    Vertex pop_first() {
        while (!first_q.empty()) {
            Vertex v = first_q.front();
            first_q.pop_front();
            if (alive[v] && in_deg[v] == 0) {
                return v;
            }
        }
        return npos;
    }

    /**
     * @brief Get one vertex from last layer (out_deg == 0)
     * Uses lazy evaluation: skips dead vertices or vertices with non-zero degree
     * @return Vertex ID or npos if no vertex available
     */
    Vertex pop_last() {
        while (!last_q.empty()) {
            Vertex v = last_q.front();
            last_q.pop_front();
            if (alive[v] && out_deg[v] == 0) {
                return v;
            }
        }
        return npos;
    }

    /**
     * @brief Get all vertices in first layer (in_deg == 0)
     * @return Vector of alive vertices with in_deg == 0
     */
    std::vector<Vertex> get_first_layer() const {
        std::vector<Vertex> result;
        result.reserve(_n);
        for (Vertex v = 0; v < _n; ++v) {
            if (alive[v] && in_deg[v] == 0) {
                result.push_back(v);
            }
        }
        return result;
    }

    /**f
     * @brief Get all vertices in last layer (out_deg == 0)
     * @return Vector of alive vertices with out_deg == 0
     */
    std::vector<Vertex> get_last_layer() const {
        std::vector<Vertex> result;
        result.reserve(_n);
        for (Vertex v = 0; v < _n; ++v) {
            if (alive[v] && out_deg[v] == 0) {
                result.push_back(v);
            }
        }
        return result;
    }

    /**
     * @brief Erase a vertex (lazy deletion)
     * Updates degrees of neighbors and adds newly degree-0 vertices to queues
     * @param v Vertex to erase
     */
    void erase(Vertex v) {
        if (v == npos) return;
        if (v >= _n) return;
        if (!alive[v]) return;

        alive[v] = false;
        --_alive_cnt;

        // Removing v deletes all incoming edges u->v: decrease out_deg[u]
        for (Vertex u : in_adj[v]) {
            if (!alive[u]) continue;
            if (out_deg[u] == 0) continue;  // already processed
            --out_deg[u];
            if (out_deg[u] == 0) {
                last_q.push_back(u);
            }
        }

        // Removing v deletes all outgoing edges v->w: decrease in_deg[w]
        for (Vertex w : out_adj[v]) {
            if (!alive[w]) continue;
            if (in_deg[w] == 0) continue;  // already processed
            --in_deg[w];
            if (in_deg[w] == 0) {
                first_q.push_back(w);
            }
        }

        // Clear degrees for tidiness
        in_deg[v]  = 0;
        out_deg[v] = 0;
    }

    /**
     * @brief Get the number of vertices (total, including dead ones)
     */
    std::size_t num_vertices() const { return _n; }

    /**
     * @brief Get the number of alive vertices
     */
    std::size_t num_alive() const { return _alive_cnt; }

    /**
     * @brief Get the number of edges (approximate, may include edges to dead vertices)
     */
    std::size_t num_edges() const {
        std::size_t count = 0;
        for (auto const& adj : out_adj) {
            count += adj.size();
        }
        return count;
    }

private:
    std::size_t _n;
    std::vector<std::vector<Vertex>> out_adj;  // Outgoing adjacency list
    std::vector<std::vector<Vertex>> in_adj;   // Incoming adjacency list
    std::vector<std::size_t> in_deg;           // In-degree for each vertex
    std::vector<std::size_t> out_deg;          // Out-degree for each vertex
    std::vector<bool> alive;                   // Whether vertex is still alive
    std::size_t _alive_cnt;                    // Number of alive vertices

    std::deque<Vertex> first_q;  // Queue of vertices with in_deg == 0
    std::deque<Vertex> last_q;   // Queue of vertices with out_deg == 0
};

}  // namespace dvlab
