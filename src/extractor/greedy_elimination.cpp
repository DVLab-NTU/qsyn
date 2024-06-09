/****************************************************************************
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/core.h>
#include <cstddef>
#include "./extract.hpp"
#include "util/util.hpp"

namespace qsyn::extractor {

/**
 * @brief find a set of addition indices which forms a row with single 1
 *
 * @param matrix
 * @param reversedSearch
 * @return vector<size_t>
 */
std::vector<size_t> Extractor::find_minimal_sums(dvlab::BooleanMatrix& matrix) {
    // NOTE - double-check directly extracted candidates and return empty result
    for (size_t i = 0; i < matrix.num_rows(); i++) {
        if (matrix[i].is_one_hot()) return {};
    }
    std::vector<std::pair<std::vector<size_t>, dvlab::BooleanMatrix::Row>> row_track_pairs, new_row_track_pairs;

    for (size_t i = 0; i < matrix.num_rows(); i++)
        row_track_pairs.emplace_back(std::vector<size_t>{i}, matrix[i]);
    std::vector<bool> neighbor_connect_to_axel(_neighbors.size(), false);
    size_t cnt = 0;
    for (auto const& n: _neighbors) {
        for (const auto& [nb, _] : _graph->get_neighbors(n)) {
            if (_axels.contains(nb)) {
                neighbor_connect_to_axel[cnt] = true;
                break;
            }
        }
        cnt++;
    }
    bool no_axel = true;
    for (size_t i=0; i< neighbor_connect_to_axel.size(); i++) {
        if (neighbor_connect_to_axel[i]) {
            no_axel = false;
            break;
        }
    }
    std::vector<size_t> first_result;
    size_t iterations = 0;
    while (true) {
        new_row_track_pairs.clear();
        for (auto const& [indices, row] : row_track_pairs) {
            auto const max_index = std::ranges::max(indices);
            for (size_t k = max_index + 1; k < matrix.num_rows(); k++) {
                auto const new_row         = row + matrix[k];
                std::vector<size_t> result = indices;
                result.emplace_back(k);
                if (new_row.is_one_hot()) {
                    return result;
                    if (no_axel) {
                        fmt::println("Return with no axel");
                        return result;
                    }
                    // The one is axel neighbor
                    for (size_t i=0; i<new_row.size(); i++) {
                        if (new_row[i] && new_row[i] == neighbor_connect_to_axel[i]) {
                            fmt::println("Return with good one");
                            return result;
                        }
                            
                    }
                    // Save to register and find another result;
                    fmt::println("Found but keep");
                    if (!first_result.empty())
                        first_result = result;
                }

                new_row_track_pairs.emplace_back(result, new_row);
                iterations++;
            }
            if (iterations > 100000) {
                spdlog::debug("Fallback to level 1");
                return {};
            }
        }
        if (new_row_track_pairs.empty())
            return {};
        // NOTE - update rowTrackPairs
        // if (!first_result.empty()) {
        //     fmt::println("Return with no sol");
        //     return first_result;
        // }
        row_track_pairs = new_row_track_pairs;
    }
}

/**
 * @brief Greedily eliminate a row to single 1
 *
 * @param matrix
 * @return vector<M2::Oper>
 */
std::vector<dvlab::BooleanMatrix::RowOperation> Extractor::greedy_reduction(dvlab::BooleanMatrix& m) {
    dvlab::BooleanMatrix matrix = m;
    std::vector<dvlab::BooleanMatrix::RowOperation> result;
    std::vector<size_t> indices = Extractor::find_minimal_sums(matrix);
    // Return empty vector if indices do not exist
    if (indices.empty()) return result;
    while (indices.size() > 1) {
        dvlab::BooleanMatrix::RowOperation best_operation(-1, -1);
        long reduction = -1 * static_cast<long>(matrix.num_cols());

        for (auto const& i : indices) {
            for (auto const& j : indices) {
                if (j <= i) continue;
                auto const new_row_sum = static_cast<long>((matrix[i] + matrix[j]).sum());
                if (int(matrix[i].sum()) - new_row_sum > reduction) {
                    // NOTE - Add j to i
                    best_operation.first  = j;
                    best_operation.second = i;
                    reduction             = static_cast<long>(matrix[i].sum()) - new_row_sum;
                }
                if (int(matrix[j].sum()) - new_row_sum > reduction) {
                    // NOTE - Add i to j
                    best_operation.first  = i;
                    best_operation.second = j;
                    reduction             = static_cast<long>(matrix[j].sum()) - new_row_sum;
                }
            }
        }
        result.emplace_back(best_operation);
        matrix[best_operation.second] = matrix[best_operation.first] + matrix[best_operation.second];

        indices.erase(remove(indices.begin(), indices.end(), best_operation.first), indices.end());
    }
    return result;
}

/**
 * @brief Find two rows with max inner product and provide the corresponding column values are both 1s
 *
 * @param matrix
 * @return Extractor::Overlap
 */
Extractor::Overlap Extractor::_max_overlap(dvlab::BooleanMatrix& matrix) {
    DVLAB_ASSERT(matrix.num_cols() == matrix.num_rows(), "The shape of input matrix should be a square.");

    size_t max_inner_product = 0;
    std::vector<size_t> best_common_indices;
    std::pair<size_t, size_t> overlap_rows(SIZE_MAX, SIZE_MAX);
    for (size_t i = 0; i < matrix.num_rows(); i++) {
        for (size_t j = i + 1; j < matrix.num_rows(); j++) {
            size_t inner_product    = 0;
            size_t num_ones_ith_row = 0;
            size_t num_ones_jth_row = 0;
            std::vector<size_t> common_indices;
            for (size_t k = 0; k < matrix.num_cols(); k++) {
                num_ones_ith_row += matrix[i][k];
                num_ones_jth_row += matrix[j][k];

                if (matrix[i][k] == 1 && matrix[j][k] == 1) {
                    inner_product++;
                    common_indices.emplace_back(k);
                }
            }

            if (inner_product > max_inner_product) {
                max_inner_product   = inner_product;
                overlap_rows        = num_ones_ith_row < num_ones_jth_row ? std::make_pair(j, i) : std::make_pair(i, j);
                best_common_indices = common_indices;
            }
        }
    }
    return {overlap_rows, best_common_indices};
}

}  // namespace qsyn::extractor
