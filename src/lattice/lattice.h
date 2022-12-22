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
        Lattice(unsigned r, unsigned c, int qs = -3, int qe = -3) : _row(r), _col(c){
            _qStart = qs;
            _qEnd = qe;
        }
        ~Lattice(){}

        // Setter and Getter
        void setRow(unsigned r)                 { _row = r; }
        void setCol(unsigned c)                 { _col = c; }
        void setQStart(size_t qs)               { _qStart = qs; }
        void setQEnd(size_t qe)                 { _qEnd = qe; }
        unsigned getRow()                       { return _row; }
        unsigned getCol()                       { return _col; }
        int getQStart()                         { return _qStart; }
        int getQEnd()                           { return _qEnd; }

        void printLT() const;

    private:
        unsigned _row;
        unsigned _col;
        int _qStart;
        int _qEnd;
};

class LTContainer{
    public:
        LTContainer(unsigned nr, unsigned nc): _nRow(nr), _nCol(nc){
            for(size_t r = 0; r < nr; r++){
                vector<Lattice*> rL;
                for(size_t c = 0; c < nc; c++){
                    Lattice* l = new Lattice(r, c);
                    rL.push_back(l);
                }
                _container.push_back(rL);
            }
        }
        ~LTContainer(){}

        void resize(unsigned r, unsigned c);
        void updateRC();

        void printLTC() const;
        void generateLTC(ZXGraph* g);
        void addCol2Right(int c);
        void addRow2Bottom(int r);
    private:
        unsigned _nRow;
        unsigned _nCol;
        vector<vector<Lattice* > > _container;

};

#endif