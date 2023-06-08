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
 * @brief
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
    vector<pair<vector<size_t>, Row>> combs, newCombs;

    for (size_t i = 0; i < matrix.numRows(); i++)
        combs.emplace_back(vector<size_t>{i}, matrix[i]);

    size_t iterations = 0;
    while (true) {
        newCombs.clear();
        for (const auto& [indices, row] : combs) {
            size_t maxIndex = *max_element(indices.begin(), indices.end());
            for (size_t k = maxIndex + 1; k < matrix.numRows(); k++) {
                Row newRow = row + matrix[k];
                vector<size_t> result = indices;
                result.push_back(k);
                if (newRow.isOneHot())
                    return result;

                newCombs.emplace_back(result, newRow);
                iterations++;
            }
            if (iterations > 100000)
                return {};
        }
        if (newCombs.empty())
            return {};
        // NOTE - update combs
        combs = newCombs;
    }
}

/**
 * @brief
 *
 * @param matrix
 * @return vector<M2::Oper>
 */
vector<M2::Oper> Extractor::greedyReduction(M2& matrix) {
    vector<M2::Oper> result;
    // TODO -
    return result;
}
