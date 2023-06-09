/****************************************************************************
  FileName     [ greedyElim.cpp ]
  PackageName  [ extractor ]
  Synopsis     [ Define class Extractor member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <assert.h>  // for assert

#include <memory>

#include "extract.h"

using namespace std;

/**
 * @brief find a set of addition indices which forms a row with single 1
 *
 * @param matrix
 * @param reversedSearch
 * @return vector<size_t>
 */
vector<size_t> Extractor::findMinimalSums(M2& matrix, bool reversedSearch) {
    // NOTE - double-check directly extracted candidates and return empty result
    for (size_t i = 0; i < matrix.numRows(); i++) {
        if (matrix[i].isOneHot()) return {};
    }
    vector<pair<vector<size_t>, Row>> rowTrackPairs, newRowTrackPairs;

    for (size_t i = 0; i < matrix.numRows(); i++)
        rowTrackPairs.emplace_back(vector<size_t>{i}, matrix[i]);

    size_t iterations = 0;
    while (true) {
        newRowTrackPairs.clear();
        for (const auto& [indices, row] : rowTrackPairs) {
            size_t maxIndex = *max_element(indices.begin(), indices.end());
            for (size_t k = maxIndex + 1; k < matrix.numRows(); k++) {
                Row newRow = row + matrix[k];
                vector<size_t> result = indices;
                result.push_back(k);
                if (newRow.isOneHot())
                    return result;

                newRowTrackPairs.emplace_back(result, newRow);
                iterations++;
            }
            if (iterations > 100000)
                return {};
        }
        if (newRowTrackPairs.empty())
            return {};
        // NOTE - update rowTrackPairs
        rowTrackPairs = newRowTrackPairs;
    }
}

/**
 * @brief Greedily eliminate a row to single 1
 *
 * @param matrix
 * @return vector<M2::Oper>
 */
vector<M2::Oper> Extractor::greedyReduction(M2& m) {
    M2 matrix = m;
    vector<M2::Oper> result;
    vector<size_t> indices = Extractor::findMinimalSums(matrix, false);
    // Return empty vector if indices do not exist
    if (!indices.size()) return result;
    while (indices.size() > 1) {
        M2::Oper bestOperation(-1, -1);
        int reduction = -1 * matrix.numCols();

        for (const auto& i : indices) {
            for (const auto& j : indices) {
                if (j <= i) continue;
                int newRowSum = (matrix[i] + matrix[j]).sum();
                if (int(matrix[i].sum()) - newRowSum > reduction) {
                    // NOTE - Add j to i
                    bestOperation.first = j;
                    bestOperation.second = i;
                    reduction = matrix[i].sum() - newRowSum;
                }
                if (int(matrix[j].sum()) - newRowSum > reduction) {
                    // NOTE - Add i to j
                    bestOperation.first = i;
                    bestOperation.second = j;
                    reduction = matrix[j].sum() - newRowSum;
                }
            }
        }
        result.push_back(bestOperation);
        matrix[bestOperation.second] = matrix[bestOperation.first] + matrix[bestOperation.second];

        indices.erase(remove(indices.begin(), indices.end(), bestOperation.first), indices.end());
    }
    return result;
}
