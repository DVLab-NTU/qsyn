// /****************************************************************************
//   FileName     [ extract.h ]
//   PackageName  [ extractor ]
//   Synopsis     [ graph extractor ]
//   Author       [ Chin-Yi Cheng ]
//   Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
// ****************************************************************************/


#ifndef EXTRACT_H
#define EXTRACT_H


#include <vector>
#include <iostream>
#include <unordered_map>

#include "zxDef.h"
#include "zxGraph.h"
#include "zxRules.h"
#include "qcirDef.h"
#include "m2.h"
#include "ordered_hashset.h"

class Extractor;

class Extractor{
    public:
        Extractor(ZXGraph* g){
            _graph = g;
        }
        ~Extractor(){ }
        
        void extract(){}
        bool removeGadget();
        void gaussianElimination();
        void updateFrontiers(){}
        void updateNeighbors(){}
        

    private:
        ZXGraph*            _graph;
        QCir*               _circuit;

        ZXVertexList        _frontiers;
        ZXVertexList        _neighbors;            

};
    
#endif