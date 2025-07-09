/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define ZigXag URL reader for ZXGraph ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_zigxag_reader.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "./zx_def.hpp"
#include "util/dvlab_string.hpp"
#include "util/phase.hpp"

namespace qsyn::zx {

namespace {

/**
 * @brief Parse vertex type from ZigXag string representation
 * 
 * @param type_str The type string from ZigXag
 * @return std::optional<VertexType> The parsed vertex type, or nullopt if invalid
 */
std::optional<VertexType> parse_zigxag_vertex_type(std::string const& type_str) {
    static std::unordered_map<std::string, VertexType> type_map = {
        {"@", VertexType::z},      // Z spider
        {"O", VertexType::x},      // X spider or Identity
        {"s", VertexType::z},      // S (Y cube) - represented as Z spider
        {"w", VertexType::x},      // W - represented as X spider
        {"in", VertexType::boundary},  // Input port
        {"out", VertexType::boundary}, // Output port
    };
    
    auto it = type_map.find(type_str);
    if (it != type_map.end()) {
        return it->second;
    }
    return std::nullopt;
}

/**
 * @brief Parse edge type from ZigXag string representation
 * 
 * @param type_str The type string from ZigXag
 * @return std::optional<EdgeType> The parsed edge type, or nullopt if invalid
 */
std::optional<EdgeType> parse_zigxag_edge_type(std::string const& type_str) {
    if (type_str == "h") {
        return EdgeType::hadamard;
    } else if (type_str == "-") {
        return EdgeType::simple;
    }
    return std::nullopt;
}

/**
 * @brief Split string by delimiter
 * 
 * @param str The string to split
 * @param delimiter The delimiter character
 * @return std::vector<std::string> Vector of substrings
 */
std::vector<std::string> split_string(std::string const& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

/**
 * @brief Parse coordinate pair from string
 * 
 * @param coord_str The coordinate string (format: "y,x")
 * @return std::optional<std::pair<float, float>> The parsed coordinates, or nullopt if invalid
 */
std::optional<std::pair<float, float>> parse_coordinates(std::string const& coord_str) {
    auto coords = split_string(coord_str, ',');
    if (coords.size() != 2) {
        return std::nullopt;
    }
    
    try {
        float y = std::stof(coords[0]);
        float x = std::stof(coords[1]);
        return std::make_pair(y, x);
    } catch (std::exception const&) {
        return std::nullopt;
    }
}

/**
 * @brief Parse node string from ZigXag format
 * 
 * @param node_str The node string (format: "y,x,type")
 * @return std::optional<std::tuple<float, float, VertexType>> The parsed node data, or nullopt if invalid
 */
std::optional<std::tuple<float, float, VertexType>> parse_node(std::string const& node_str) {
    auto parts = split_string(node_str, ',');
    if (parts.size() != 3) {
        return std::nullopt;
    }
    
    auto coords = parse_coordinates(parts[0] + "," + parts[1]);
    if (!coords.has_value()) {
        return std::nullopt;
    }
    
    auto vertex_type = parse_zigxag_vertex_type(parts[2]);
    if (!vertex_type.has_value()) {
        return std::nullopt;
    }
    
    return std::make_tuple(coords->first, coords->second, *vertex_type);
}

/**
 * @brief Parse edge string from ZigXag format
 * 
 * @param edge_str The edge string (format: "y1,x1,y2,x2,type")
 * @return std::optional<std::tuple<float, float, float, float, EdgeType>> The parsed edge data, or nullopt if invalid
 */
std::optional<std::tuple<float, float, float, float, EdgeType>> parse_edge(std::string const& edge_str) {
    auto parts = split_string(edge_str, ',');
    if (parts.size() != 5) {
        return std::nullopt;
    }
    
    try {
        float y1 = std::stof(parts[0]);
        float x1 = std::stof(parts[1]);
        float y2 = std::stof(parts[2]);
        float x2 = std::stof(parts[3]);
        
        auto edge_type = parse_zigxag_edge_type(parts[4]);
        if (!edge_type.has_value()) {
            return std::nullopt;
        }
        
        return std::make_tuple(y1, x1, y2, x2, *edge_type);
    } catch (std::exception const&) {
        return std::nullopt;
    }
}

}  // namespace

std::optional<ZXGraph> from_zigxag_url(std::string const& zigxag_url) {
    // Extract the part after the hash symbol
    size_t hash_pos = zigxag_url.find('#');
    if (hash_pos == std::string::npos) {
        spdlog::error("Invalid ZigXag URL format: missing '#' symbol");
        spdlog::error("Expected format: https://algassert.com/zigxag#nodes:edges");
        return std::nullopt;
    }
    
    std::string zigxag_str = zigxag_url.substr(hash_pos + 1);
    return from_zigxag_string(zigxag_str);
}

std::optional<ZXGraph> from_zigxag_string(std::string const& zigxag_str) {
    // Split into nodes and edges parts
    size_t colon_pos = zigxag_str.find(':');
    if (colon_pos == std::string::npos) {
        spdlog::error("Invalid ZigXag string format: missing ':' separator");
        spdlog::error("Expected format: nodes:edges");
        return std::nullopt;
    }
    
    std::string nodes_str = zigxag_str.substr(0, colon_pos);
    std::string edges_str = zigxag_str.substr(colon_pos + 1);
    
    // Check if strings are empty
    if (nodes_str.empty()) {
        spdlog::error("Invalid ZigXag string: nodes part is empty");
        return std::nullopt;
    }
    
    if (edges_str.empty()) {
        spdlog::error("Invalid ZigXag string: edges part is empty");
        return std::nullopt;
    }
    
    ZXGraph graph;
    std::unordered_map<std::string, ZXVertex*> coord_to_vertex;
    std::unordered_map<std::string, QubitIdType> coord_to_qubit;
    std::unordered_map<float, QubitIdType> y_to_qubit;  // Map y-coordinate to qubit ID
    QubitIdType next_qubit_id = 0;
    
    // Parse nodes
    auto node_tokens = split_string(nodes_str, ';');
    if (node_tokens.empty()) {
        spdlog::error("No nodes found in ZigXag string");
        return std::nullopt;
    }
    
    // First pass: collect all boundary vertices to determine qubit assignments
    std::vector<std::tuple<float, float, std::string>> boundary_vertices;  // y, x, type
    for (auto const& node_token : node_tokens) {
        auto parts = split_string(node_token, ',');
        if (parts.size() >= 3 && (parts[2] == "in" || parts[2] == "out")) {
            try {
                float y = std::stof(parts[0]);
                float x = std::stof(parts[1]);
                boundary_vertices.emplace_back(y, x, parts[2]);
            } catch (std::exception const&) {
                spdlog::error("Failed to parse boundary vertex coordinates: {}", node_token);
                return std::nullopt;
            }
        }
    }
    
    // Assign qubit IDs based on y-coordinate (qubit line)
    for (auto const& [y, x, type] : boundary_vertices) {
        if (y_to_qubit.find(y) == y_to_qubit.end()) {
            y_to_qubit[y] = next_qubit_id++;
        }
    }
    
    // Second pass: create all vertices
    for (auto const& node_token : node_tokens) {
        auto node_data = parse_node(node_token);
        if (!node_data.has_value()) {
            spdlog::error("Failed to parse node: {}", node_token);
            return std::nullopt;
        }
        
        auto [y, x, vertex_type] = *node_data;
        // Use inverted x-coordinate for coordinate key to match the vertex positioning
        std::string coord_key = std::to_string(y) + "," + std::to_string(-x);
        
        ZXVertex* vertex = nullptr;
        if (vertex_type == VertexType::boundary) {
            // Determine if it's input or output based on the type string
            auto parts = split_string(node_token, ',');
            if (parts.size() >= 3 && parts[2] == "in") {
                QubitIdType qubit_id = y_to_qubit[y];
                // Invert x-coordinate to fix the left/right positioning
                vertex = graph.add_input(qubit_id, y, -x);
                coord_to_qubit[coord_key] = qubit_id;
            } else if (parts.size() >= 3 && parts[2] == "out") {
                QubitIdType qubit_id = y_to_qubit[y];
                // Invert x-coordinate to fix the left/right positioning
                vertex = graph.add_output(qubit_id, y, -x);
                coord_to_qubit[coord_key] = qubit_id;
            } else {
                spdlog::error("Invalid boundary vertex type: {}", node_token);
                return std::nullopt;
            }
        } else {
            // Invert x-coordinate for all vertices to maintain consistency
            vertex = graph.add_vertex(vertex_type, dvlab::Phase(0), y, -x);
        }
        
        coord_to_vertex[coord_key] = vertex;
    }
    
    // Parse edges
    auto edge_tokens = split_string(edges_str, ';');
    for (auto const& edge_token : edge_tokens) {
        if (edge_token.empty()) continue; // Skip empty tokens
        
        auto edge_data = parse_edge(edge_token);
        if (!edge_data.has_value()) {
            spdlog::error("Failed to parse edge: {}", edge_token);
            return std::nullopt;
        }
        
        auto [y1, x1, y2, x2, edge_type] = *edge_data;
        // Use inverted x-coordinates for coordinate keys to match the vertex positioning
        std::string coord1_key = std::to_string(y1) + "," + std::to_string(-x1);
        std::string coord2_key = std::to_string(y2) + "," + std::to_string(-x2);
        
        auto it1 = coord_to_vertex.find(coord1_key);
        auto it2 = coord_to_vertex.find(coord2_key);
        
        if (it1 == coord_to_vertex.end()) {
            spdlog::error("Edge references non-existent vertex at coordinates ({}, {}): {}", y1, x1, edge_token);
            return std::nullopt;
        }
        
        if (it2 == coord_to_vertex.end()) {
            spdlog::error("Edge references non-existent vertex at coordinates ({}, {}): {}", y2, x2, edge_token);
            return std::nullopt;
        }
        
        graph.add_edge(it1->second, it2->second, edge_type);
    }
    
    spdlog::debug("Successfully parsed ZigXag string: {} nodes, {} edges", 
                  node_tokens.size(), edge_tokens.size());
    
    return graph;
}

}  // namespace qsyn::zx 