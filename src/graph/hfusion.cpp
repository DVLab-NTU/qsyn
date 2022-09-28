/***************************voi*************************************************
  FileName     [ hfusion.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Hadamard Cancellation Rule Definition ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/


#include <iostream>
#include <vector>
#include "zxRules.h"
using namespace std;

extern size_t verbose;

/**
 * @brief Matches Hadamard-edges that are connected to H-boxes or two eighboring H-boxes
 *        (Check PyZX/pyzx/hrules.py/match_connected_hboxes for more details)
 * 
 * @param g 
 */
void HboxFusion::match(ZXGraph* g){
  _matchTypeVec.clear();
  //TODO: rewrite _matchTypeVec
}


/**
 * @brief Generate Rewrite format from `_matchTypeVec`
 *        (Check PyZX/pyzx/hrules.py/fuse_hboxes for more details)
 * 
 * @param g 
 */
void HboxFusion::rewrite(ZXGraph* g){
    reset();
    //TODO: Rewrite _removeVertices, _removeEdges, _edgeTableKeys, _edgeTableValues
    //* _removeVertices: all ZXVertex* must be removed from ZXGraph this cycle.
    //* _removeEdges: all EdgePair must be removed from ZXGraph this cycle.
    //* (EdgeTable: Key(ZXVertex* vs, ZXVertex* vt), Value(int s, int h))
    //* _edgeTableKeys: A pair of ZXVertex* like (ZXVertex* vs, ZXVertex* vt), which you would like to add #s EdgeType::SIMPLE between them and #h EdgeType::HADAMARD between them
    //* _edgeTableValues: A pair of int like (int s, int h), which means #s EdgeType::SIMPLE and #h EdgeType::HADAMARD

}