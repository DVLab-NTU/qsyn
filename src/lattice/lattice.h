/****************************************************************************
  FileName     [ lattice.h ]
  PackageName  [ lattice ]
  Synopsis     [ Lattice Surgery structure definition ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef LATTICE_H
#define LATTICE_H

#include <vector>
#include <iostream>

#include "zxDef.h"
#include "zxGraph.h"


class Lattice;
class LTContainer;

class Lattice{
    public:
        Lattice(unsigned r, unsigned c, size_t qs = -1, size_t qe = -1) : _row(r), _col(c){
            _qStart = qs;
            _qEnd = qe;
        }
        ~Lattice(){}

        // Setter and Getter
        unsigned setRow(unsigned r)             { _row = r; }
        unsigned setCol(unsigned c)             { _col = c; }
        size_t setQStart(size_t qs)             { _qStart = qs; }
        size_t setQEnd(size_t qe)               { _qEnd = qe; }
        unsigned getRow()                       { return _row; }
        unsigned getCol()                       { return _col; }
        size_t getQStart()                      { return _qStart; }
        size_t getQEnd()                        { return _qEnd; }

    private:
        unsigned _row;
        unsigned _col;
        size_t _qStart;
        size_t _qEnd;
};

class LTContainer{
    public:
        LTContainer(unsigned nr, unsigned nc) _nRow(nr), _nCol(nc){
            for(size_t r = 0; i < nr; r++){
                vector<Lattice*> rL;
                for(size_t c = 0; c < nc; c++){
                    Lattice* l = new Lattice(r, c);
                    rL.push_back(l);
                }
                _container.push_back(rL);
            }
        }
        ~LTContainer(){}
    private:
        unsigned _nRow;
        unsigned _nCol;
        vector<vector<Lattice* > > _container;

};

#endif