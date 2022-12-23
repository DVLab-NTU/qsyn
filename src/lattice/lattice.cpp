/****************************************************************************
  FileName     [ lattice.cpp ]
  PackageName  [ lattice ]
  Synopsis     [ Define class Lattice / LTContainer member functions ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iomanip>
#include <unordered_set>
#include "lattice.h"
#include "textFormat.h"

namespace TF = TextFormat;

using namespace std;
extern size_t verbose;


void printMap(unordered_map<int, unordered_set<int>> map){
    for(auto& s : map){
        cout << s.first << ": ";
        for(auto& v : s.second) cout << v << ", ";
        cout << endl;
    }
}

void Lattice::printLT() const{
    cout << "( " << _row << ", " << _col << " ): " << _qStart << "/" << _qEnd << endl;
}

void LTContainer::resize(unsigned r, unsigned c){
    _nRow = r; _nCol = c;
    for(size_t r = 0; r < _nRow; r++){
        vector<Lattice*> rL;
        for(size_t c = 0; c < _nCol; c++){
            Lattice* l = new Lattice(r, c);
            rL.push_back(l);
        }
        _container.push_back(rL);
    }
}

void LTContainer::printLTC() const {
    for(size_t c = 0; c < _nCol; c++){
        if(c != _nCol-1) cout << setw(5) << right << c << setw(5) << right << "|";
        else cout << setw(5) << right << c << setw(5) << right << "|" << endl;
    } 
    for(size_t r = 0; r < _nRow; r++){
        for(size_t c = 0; c < _nCol; c++){
            if(c != _nCol-1) cout << setw(4) << right << ((_container[r][c] -> getQStart() == -3) ? "-" : to_string(_container[r][c] -> getQStart())) << "/" << ((_container[r][c] -> getQEnd() == -3) ? "-" : to_string(_container[r][c] -> getQEnd())) << setw(4) << right << "|";
            else cout << setw(4) << right << ((_container[r][c] -> getQStart() == -3) ? "-" : to_string(_container[r][c] -> getQStart())) << "/" << ((_container[r][c] -> getQEnd() == -3)? "-" : to_string(_container[r][c] -> getQEnd())) << setw(4) << right << "|" << endl;
        }
    }
}



void LTContainer::updateRC(){
    for(size_t i = 0; i < _container.size(); i++){
        for(size_t j = 0; j < _container[i].size(); j++){
            _container[i][j]->setRow(i);
            _container[i][j]->setCol(j);
        }
    }
}

void LTContainer::addCol2Right(int c){
    _nCol++;
    if(c >= (int)_nCol-1){
        // Add to the right of the container
        for(size_t r = 0; r < _nRow; r++){
            Lattice* l = new Lattice(r, _nCol-1);
            _container[r].push_back(l);
        }
    }
    else if(c < 0){
        // Add to the left of the container
        for(size_t r = 0; r < _nRow; r++){
            Lattice* l = new Lattice(r, 0);
            _container[r].insert(_container[r].begin(), l);
        }
        updateRC();
    }
    else{
        // Add in the middle of the container (right of the original `c`)
        for(size_t r = 0; r < _nRow; r++){
            Lattice* l = new Lattice(r, _nCol-1);
            _container[r].insert(_container[r].begin() + c + 1, l);
        }
        updateRC();
    }
}

void LTContainer::addRow2Bottom(int r){
    _nRow++;
    if(r < 0){
        // Add at the top of the container
        vector<Lattice* > tmp;
        for(size_t i = 0; i < _nCol; i++){
            Lattice* l = new Lattice(0, i);
            tmp.push_back(l);
        }
        _container.insert(_container.begin(), tmp);
        updateRC();
    }
    else if(r >= (int)_nRow-1){
        // Add at the bottom of the container
        vector<Lattice* > tmp;
        for(size_t i = 0; i < _nCol; i++){
            Lattice* l = new Lattice(_nRow-1, i);
            tmp.push_back(l);
        }
        _container.push_back(tmp);
    }
    else{
        // Add in the middle of the container
        vector<Lattice* > tmp;
        for(size_t i = 0; i < _nCol; i++){
            Lattice* l = new Lattice(_nRow-1, i);
            tmp.push_back(l);
        }
        _container.insert(_container.begin()+r+1,tmp);
        updateRC();
    }
}

void LTContainer::generateLTC(ZXGraph* g){
    // Prerequisite:
    // Input col: 0
    // Output col: all equivalent and be the max col in `g`
    // Odd col: X , Even col: Z -> Col[1,2] as a unit
    // Not allow empty cols 
    // `g` is consisted of several pairs of unit's composition

    vector<vector<ZXVertex*> > colGroup;
    unsigned int maxCol = 0;
    for(auto & o : g->getOutputs()){
        if(o->getCol() > maxCol) maxCol = o->getCol();
    }
    cout << "MaxCol: " << maxCol << endl;

    colGroup.resize(maxCol+1);
    for(auto & v: g->getVertices()){
        colGroup[v->getCol()].push_back(v);
    }
    // Print
    // for(size_t i = 0; i < colGroup.size(); i++){
    //     cout << "Col " << i << ": ";
    //     for(size_t j = 0; j < colGroup[i].size(); j++){
    //         cout << colGroup[i][j]->getId() << " ";
    //     }
    //     cout << endl;
    // }
    if(!(maxCol % 2)) return;
    for(unsigned int i = 1; i <= maxCol/2; i++){
        unordered_map<int, unordered_set<int>> start, end;
        
        // Generate start
        for(auto& v : colGroup[2*i-1]){
            unordered_set<int> tmp;
            start[v->getQubit()] = tmp;
            for(auto& n : v->getNeighbors()){
                if(n.first->getCol() > v->getCol()) start[v->getQubit()].insert(n.first->getQubit());
            }
        }

        // Generate End
        for(auto& v : colGroup[2*i]){
            unordered_set<int> tmp;
            end[v->getQubit()] = tmp;
            for(auto& n : v->getNeighbors()){
                if(n.first->getCol() < v->getCol()) end[v->getQubit()].insert(n.first->getQubit());
            }
        }

        if(verbose > 3){
            cout << "Start:" << endl;
            printMap(start);
            cout << "End:" << endl;
            printMap(end);
        }

        // Start mapping to LTC
        resize(end.size()+1, start.size()+1);
        unordered_map<string, pair<int, int> > set;
        int count = 0;
        for(auto itr = start.begin(); itr != start.end(); itr++){
            cout << itr->first << endl;
            for(auto& item : itr->second){
                string str = to_string(itr->first) +","+to_string(item);
                set[str] = make_pair(count, -1);
            }
            count++;
        }
        count = 0;
        for(auto& e : end){
            for(auto& s : e.second){
                string str = to_string(s)+","+to_string(e.first);
                if(set.find(str) != set.end()) set[str].second = count;
                else set[str] = make_pair(-1, count);
            }
            count++;
        }
        vector<pair<string, pair<int, int> > > sCand, eCand;
        for(auto& s : set){
            if(s.second.first != -1 && s.second.second != -1){
                size_t p = s.first.find_first_of(",",0);
                int x = stoi(s.first.substr(0, p));
                int y = stoi(s.first.substr(p+1, s.first.size()-p-1));
                _container[s.second.second][s.second.first]->setQStart(x);
                _container[s.second.second][s.second.first]->setQEnd(y);
            } 
            else if(s.second.first == -1) eCand.push_back(s);
            else if(s.second.second == -1) sCand.push_back(s);
        }
        
        // Compensate vertically
        addRow2Bottom(-1);
        for(auto& s : sCand){
            if(verbose > 3) cout << s.first << ": (" << s.second.first << "," << s.second.second << ")\n";
            if(_container[0][s.second.first]->getQStart() == -3 && _container[0][s.second.first]->getQEnd() == -3){
                // Find first not (-3, -3)
                for(size_t x = 1; x < _container.size(); x++){
                    if(_container[x][s.second.first]->getQStart() != -3 && _container[x][s.second.first]->getQEnd() != -3){
                        size_t p = s.first.find_first_of(",",0);
                        int qs = stoi(s.first.substr(0, p));
                        int qe = stoi(s.first.substr(p+1, s.first.size()-p-1));
                        _container[x-1][s.second.first]->setQStart(qs);
                        _container[x-1][s.second.first]->setQEnd(qe);
                        break;
                    }
                }
            }
            else{
                // Find first (-3, -3)
                for(size_t x = 1; x < _container.size(); x++){
                    if(_container[x][s.second.first]->getQStart() == -3 && _container[x][s.second.first]->getQEnd() == -3){
                        size_t p = s.first.find_first_of(",",0);
                        int qs = stoi(s.first.substr(0, p));
                        int qe = stoi(s.first.substr(p+1, s.first.size()-p-1));
                        _container[x][s.second.first]->setQStart(qs);
                        _container[x][s.second.first]->setQEnd(qe);
                        break;
                    }
                }
            }
        }
        
        // Compensate horizontally
        addCol2Right(-1);
        for(auto& s : eCand){
            s.second.second++;
            cout << s.first << ": (" << s.second.first << "," << s.second.second << ")\n";
            size_t p = s.first.find_first_of(",",0);
            int qs = stoi(s.first.substr(0, p));
            int qe = stoi(s.first.substr(p+1, s.first.size()-p-1));
            for(size_t x = 1; x < _container[0].size(); x++){
                if(_container[s.second.second][x]->getQStart() != -3 && _container[s.second.second][x]->getQEnd() != -3){
                    _container[s.second.second][x-1]->setQStart(qs);
                    _container[s.second.second][x-1]->setQEnd(qe);
                    break;
                }
            }
        }
    }
}


