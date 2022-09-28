/****************************************************************************
  FileName     [ bialg.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Bialgebra Rule Definition ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#include <iostream>
#include <vector>
#include "zxRules.h"
using namespace std;

extern size_t verbose;

/**
 * @brief Finds noninteracting matchings of the bialgebra rule. 
 *        (Check PyZX/pyzx/rules.py/match_bialg_parallel for more details)
 * 
 * @param g 
 */
void Bialgebra::match(ZXGraph* g){
  _matchTypeVec.clear();
  //TODO: rewrite _matchTypeVec
}

/**
 * @brief Performs a certain type of bialgebra rewrite based on `_matchTypeVec`
 *        (Check PyZX/pyzx/rules.py/bialg for more details)
 * 
 * @param g 
 */
void Bialgebra::rewrite(ZXGraph* g){
    reset();
    //TODO: Rewrite _removeVertices, _removeEdges, _edgeTableKeys, _edgeTableValues
    //* _removeVertices: all ZXVertex* must be removed from ZXGraph this cycle.
    //* _removeEdges: all EdgePair must be removed from ZXGraph this cycle.
    //* (EdgeTable: Key(ZXVertex* vs, ZXVertex* vt), Value(int s, int h))
    //* _edgeTableKeys: A pair of ZXVertex* like (ZXVertex* vs, ZXVertex* vt), which you would like to add #s EdgeType::SIMPLE between them and #h EdgeType::HADAMARD between them
    //* _edgeTableValues: A pair of int like (int s, int h), which means #s EdgeType::SIMPLE and #h EdgeType::HADAMARD

}