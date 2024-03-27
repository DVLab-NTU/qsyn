/****************************************************************************
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <memory>

#include "./extract.hpp"

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

    size_t iterations = 0;
    while (true) {
        new_row_track_pairs.clear();
        for (auto const& [indices, row] : row_track_pairs) {
            auto const max_index = std::ranges::max(indices);
            for (size_t k = max_index + 1; k < matrix.num_rows(); k++) {
                auto const new_row         = row + matrix[k];
                std::vector<size_t> result = indices;
                result.emplace_back(k);
                if (new_row.is_one_hot())
                    return result;

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

}  // namespace qsyn::extractor
