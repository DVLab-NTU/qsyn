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
#include "simplify.h"
#include "qcir.h"
#include "m2.h"
#include "ordered_hashset.h"

class Extractor;

class Extractor{
    public:
        Extractor(ZXGraph* g){
            _graph = g;
            _circuit = new QCir(-1);
            initialize();
        }
        ~Extractor(){ }
        
        void initialize();
        void extract(){}
        bool removeGadget();
        void gaussianElimination();

        void extractSingles();
        void extractCZs(size_t=0);
        void cleanFrontier();
        void updateNeighbors();
        
        void printFroniters();
        void printNeighbors();
        void printAxels();

    private:
        ZXGraph*            _graph;
        QCir*               _circuit;
        ZXVertexList        _frontier;
        ZXVertexList        _neighbors;   
        ZXVertexList        _axels;   
        unordered_map<size_t, size_t> _qubitMap;        
};
    
#endif