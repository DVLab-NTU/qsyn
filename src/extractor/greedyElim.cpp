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
vector<M2::Oper> Extractor::greedyReduction(M2& m) {
    M2 matrix = m;
    vector<M2::Oper> result;
    vector<size_t> indicest = Extractor::findMinimalSums(matrix, false);
    // Return empty vector if indicest do not exist
    if (!indicest.size()) return result;

    while(!indicest.size()){
        M2::Oper best_oper(-1, -1);
        size_t reduction = -1*matrix.numCols();

        for (size_t i = 0; i < indicest.size(); i++){
            for (size_t j=i+1; j < indicest.size(); j++){
                size_t new_row_sum = (matrix[i]+matrix[j]).sum();

                if(matrix[i].sum()-new_row_sum > reduction){
                    //NOTE - Add j to i
                    best_oper.first = j;
                    best_oper.second = i;
                    reduction = matrix[i].sum()-new_row_sum;
                }
                if(matrix[j].sum()-new_row_sum > reduction){
                    //NOTE - Add i to j
                    best_oper.first = i;
                    best_oper.second = j;
                    reduction = matrix[j].sum()-new_row_sum;
                }
            }
        }
        result.push_back(best_oper);
        matrix[best_oper.second] = matrix[best_oper.first] + matrix[best_oper.second];
        
        indicest.erase(std::remove(indicest.begin(), indicest.end(), best_oper.first), indicest.end());
    }


    return result;
}
