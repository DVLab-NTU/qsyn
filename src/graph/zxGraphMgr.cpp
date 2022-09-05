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

void ZXGraphMgr::copy(size_t id){
  if(_graphList.empty()) cerr << "Error: ZXGraphMgr is empty now! Action \"copy\" failed!" << endl;
  else{
    ZXGraph* copyTarget;
    if(isID(id)){
      // Overwrite existed ZXGraph
      cout << "Overwrite existed Graph " << id << endl;
      copyTarget = findZXGraphByID(id);
      copyTarget->setEdges(getGraph()->getEdges());
      copyTarget->setInputs(getGraph()->getInputs());
      copyTarget->setOutputs(getGraph()->getOutputs());
      copyTarget->setVertices(getGraph()->getVertices());
      copyTarget->setQubitCount(getGraph()->getQubitCount());

      cout << "Successfully copy Graph " << getGraph()->getId() << " to Graph "<< id << endl;
      checkout2ZXGraph(id);
    }
    else{
      // Create a new ZXGraph
      size_t oriGraphID = getGraph()->getId();
      copyTarget = new ZXGraph(id);
      copyTarget->setEdges(getGraph()->getEdges());
      copyTarget->setInputs(getGraph()->getInputs());
      copyTarget->setOutputs(getGraph()->getOutputs());
      copyTarget->setVertices(getGraph()->getVertices());
      copyTarget->setQubitCount(getGraph()->getQubitCount());
      _graphList.push_back(copyTarget);
      _gListItr = _graphList.end()-1;
      if(id == _nextID || _nextID < id) _nextID = id + 1;
      cout << "Successfully copy Graph " << oriGraphID << " to Graph "<< id << endl;
      cout << "Checkout to Graph " << id << endl;
    }
  }
}

ZXGraph* ZXGraphMgr::findZXGraphByID(size_t id) const{
  if(!isID(id)) cerr << "Error: Graph " << id << " is not exist!" << endl;
  else{
    for(size_t i = 0; i < _graphList.size(); i++){
      if(_graphList[i]->getId() == id) return _graphList[i];
    }
  }
  return nullptr;
}



// Print
void ZXGraphMgr::printZXGraphMgr() const{
  cout << "-> #Graph: " << _graphList.size() << endl;
  if(!_graphList.empty()) cout << "-> Now focus on: " << getGraph()->getId() << endl;
}

void ZXGraphMgr::printGListItr() const{
  if(!_graphList.empty()) cout << "Now focus on: " << getGraph()->getId() << endl;
  else cerr << "Error: ZXGraphMgr is empty now!" << endl;
}

void ZXGraphMgr::printGraphListSize() const{
  cout << "#Graph: " << _graphList.size() << endl;
}




