/****************************************************************************
  FileName     [ zxGraphMgr.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define ZX-graph manager ]
  Author       [ Cheng-Hua Lu ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>
#include <vector>
#include "zxGraphMgr.h"
using namespace std;

ZXGraphMgr* zxGraphMgr = 0;

/*****************************************/
/*   class ZXGraphMgr member functions   */
/*****************************************/

void ZXGraphMgr::reset(){
    _graphList.clear();
    _gListItr = _graphList.begin();
    _nextID = 0;
}

// Test

bool ZXGraphMgr::isID(size_t id) const{
  for(size_t i = 0; i < _graphList.size(); i++){
    if(_graphList[i]->getId() == id) return true;
  }
  return false;
}

// Add and Remove

void ZXGraphMgr::addZXGraph(size_t id){
  ZXGraph* zxGraph = new ZXGraph(id);
  _graphList.push_back(zxGraph);
  _gListItr = _graphList.end()-1;
  if(id == _nextID || _nextID < id) _nextID = id + 1;
  cout << "Successfully generate Graph " << id << endl;
  cout << "Checkout to Graph " << id << endl;
}

void ZXGraphMgr::removeZXGraph(size_t id){
  for(size_t i = 0; i < _graphList.size(); i++){
    if(_graphList[i]->getId() == id){
      _graphList.erase(_graphList.begin() + i);
      cout << "Successfully remove Graph " << id << endl;
      _gListItr = _graphList.begin();
      if(!_graphList.empty()) cout << "Checkout to Graph " << _graphList[0]->getId() << endl;
      else cout << "Warning: The graph list is empty now" << endl;
      return;
    }
  }
  cerr << "Error: The id provided is not exist!!" << endl;
  return;
}


// Action

void ZXGraphMgr::checkout2ZXGraph(size_t id){
  for(size_t i = 0; i < _graphList.size(); i++){
    if(_graphList[i]->getId() == id){
      _gListItr = _graphList.begin() + i;
      cout << "Checkout to Graph " << id << endl;
      return;
    }
  }
  cerr << "Error: The id provided is not exist!!" << endl;
  return;
}




