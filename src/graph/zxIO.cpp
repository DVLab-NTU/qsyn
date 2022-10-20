/****************************************************************************
  FileName     [ zxIO.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define qcir reader functions ]
  Author       [ Chin-Yi Cheng, Yi-Hsiang Kuo ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <vector>
#include <string>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cassert>
#include "zxGraph.h"
#include <unordered_map>

extern size_t verbose;

/// @brief read a zx graph
/// @param filename 
/// @return true if correctly consturct the graph
bool ZXGraph::readZX(string filename){
  fstream ZXFile;
  ZXFile.open(filename.c_str(), ios::in);
  if (!ZXFile.is_open()){
    cerr << "Cannot open the file \"" << filename << "\"!!" << endl;
    return false;
  }
  string line;
  string token;
  unordered_map<size_t, vector<pair<size_t, EdgeType>>> storage;
  unordered_map<size_t, ZXVertex*> VertexList;
  size_t counter = 1;
  while(getline(ZXFile, line)){
    //Slice comment
    size_t found = line.find("//");
    if(found!=string::npos)
      line = line.substr(0, found);
    if(line == "") continue;
    vector<string> tokens;
    size_t n = myStrGetTok(line, token);
    while (token.size()) {
      tokens.push_back(token);
      n = myStrGetTok(line, token, n);
    }
    unsigned id, qid, nid;
    string vertexStr = tokens[0];
    if(vertexStr[0] == 'I' || vertexStr[0] == 'O'){
      string idStr = vertexStr.substr(1);
      
      if(!myStr2Uns(idStr, id)){
        cerr << "Error: Vertex Id " << idStr << " is not an unsigned in line " << counter << "!!" << endl;
        return false;
      }
      if (storage.find(size_t(id)) != storage.end()) {
        cerr << "Error: Duplicated vertex Id " << size_t(id) << " in line " << counter << "!!" << endl;
        return false;
      }
      if(tokens.size()==0){
        cerr << "Error: Missing qubit Id after I/O declaration in line " << counter << "!!" << endl;
        return false;
      }
      if(!myStr2Uns(tokens[1], qid)){
        cerr << "Error: Qubit Id " << tokens[1] << " is not an unsigned in line " << counter << "!!" << endl;
        return false;
      }
      vector<pair<size_t, EdgeType>> tmp;
      for(size_t s=2; s<tokens.size(); s++){
        string neighborStr = tokens[s];
        if(neighborStr[0] == 'S' || neighborStr[0] == 'H'){
          string nidStr = neighborStr.substr(1);
          if(!myStr2Uns(nidStr, nid)){
            cerr << "Error: Vertex Id in declaring neighbor " << neighborStr << " is not an unsigned in line " << counter << "!!" << endl;
            return false;
          }
          else if(size_t(nid) >= size_t(id))
            tmp.push_back(make_pair(size_t(nid), (neighborStr[0] == 'S') ? EdgeType::SIMPLE : EdgeType::HADAMARD));
        }
        else{
          cerr << "Error: Unsupported edge type " << neighborStr[0] << " in line " << counter << "!!" << endl;
          return false;
        }
      }
      storage[size_t(id)] = tmp;
      if(vertexStr[0] == 'I'){
        VertexList[size_t(id)] = addInput(size_t(id), size_t(qid));
      }
        
      else
        VertexList[size_t(id)] = addOutput(size_t(id), size_t(qid));
    }
    else if(vertexStr[0] == 'Z' || vertexStr[0] == 'X' || vertexStr[0] == 'H'){
      string idStr = vertexStr.substr(1);

      if(!myStr2Uns(idStr, id)){
        cerr << "Error: Vertex Id " << idStr << " is not an unsigned in line " << counter << "!!" << endl;
        return false;
      }
      if (storage.find(size_t(id)) != storage.end()) {
        cerr << "Error: Duplicated vertex Id " << size_t(id) << " in line " << counter << "!!" << endl;
        return false;
      }
      vector<pair<size_t, EdgeType>> tmp;
      Phase ph;
      for(size_t s=1; s<tokens.size(); s++){
        string neighborStr = tokens[s];
        bool checkNeighbor = true;
        if(s==tokens.size()-1){
          if(ph.fromString(neighborStr)){
            checkNeighbor = false;
          }
        }
        if(checkNeighbor){
          if(neighborStr[0] == 'S' || neighborStr[0] == 'H'){
            string nidStr = neighborStr.substr(1);
            if(!myStr2Uns(nidStr, nid)){
              cerr << "Error: Vertex Id in declaring neighbor " << neighborStr << " is not an unsigned in line " << counter << "!!" << endl;
              return false;
            }
            else if(size_t(nid) >= size_t(id))
              tmp.push_back(make_pair(size_t(nid), (neighborStr[0] == 'S') ? EdgeType::SIMPLE : EdgeType::HADAMARD));
          }
          else{
            cerr << "Error: Unsupported edge type " << neighborStr[0] << " in line " << counter << "!!" << endl;
            return false;
          }
        }
      }
      storage[size_t(id)] = tmp;
      if(vertexStr[0] == 'Z')
        VertexList[size_t(id)] = addVertex(size_t(id), size_t(0), VertexType::Z, ph);
      else if(vertexStr[0] == 'X')
        VertexList[size_t(id)] = addVertex(size_t(id), size_t(0), VertexType::X, ph);
      else 
        VertexList[size_t(id)] = addVertex(size_t(id), size_t(0), VertexType::H_BOX, ph);
    }
    else{
      cerr << "Error: Unsupported vertex type " << vertexStr[0] << " in line " << counter << "!!" << endl;
      return false;
    }
    
    counter++;
  }
  
  for(auto itr = storage.begin(); itr!=storage.end(); itr++){
    vector<pair<size_t, EdgeType>> neighbor = itr->second;
    for(size_t nb=0; nb<neighbor.size(); nb++){
      size_t nbId = neighbor[nb].first;
      EdgeType edgeType = neighbor[nb].second;
      if(VertexList[nbId]==nullptr){
        cerr << "Found a never declared id " << nbId << " in neighbor list of vertex " << itr->first << "!!" << endl;
        return false;
      } 
      else{
        VertexList[nbId];
        addEdge(VertexList[itr->first], VertexList[nbId], &edgeType);
      }
    }
  }
  if(verbose>=3) printVertices();
  if(verbose>=3) printEdges();
  return true;
}


/// @brief write a zxgraph
/// @param filename 
/// @return true if correctly write a graph into .zx
bool ZXGraph::writeZX(string filename){
  fstream ZXFile;
  ZXFile.open(filename.c_str(), std::fstream::out);
  if (!ZXFile.is_open()){
    cerr << "Cannot open the file \"" << filename << "\"!!" << endl;
    return false;
  }
  ZXFile << "// Input \n";
  for(size_t i=0; i<_inputs.size(); i++){
    ZXVertex* v = _inputs[i];
    ZXFile << "I" << v->getId() << " " << v->getQubit();
    NeighborMap nbm = v->getNeighborMap();
    
    for(auto itr=nbm.begin(); itr!=nbm.end(); itr++){
      if(itr->first->getId() >= v->getId()){
        // cout << "VV:::  "<< (itr->second) << endl;
        cout << "II:::  "<< (itr->second) << endl;
        ZXFile << " ";
        if(*(itr->second)==EdgeType::SIMPLE)
          ZXFile << "S";
        else if(*(itr->second)==EdgeType::HADAMARD)
          ZXFile << "H";
        else{
          cerr << "Error: The edge type is ERRORTYPE" << endl;
          // return false;
        }
        ZXFile << itr->first->getId();
      }
    }
    ZXFile << "\n";
  }
  ZXFile << "// Output \n";
  for(size_t i=0; i<_outputs.size(); i++){
    ZXVertex* v = _outputs[i];
    ZXFile << "O" << v->getId() << " " << v->getQubit();
    NeighborMap nbm = v->getNeighborMap();
    
    for(auto itr=nbm.begin(); itr!=nbm.end(); itr++){
      if(itr->first->getId() >= v->getId()){
        cout << "OO:::  "<< (itr->second) << endl;
        ZXFile << " ";
        if(*(itr->second)==EdgeType::SIMPLE)
          ZXFile << "S";
        else if(*(itr->second)==EdgeType::HADAMARD)
          ZXFile << "H";
        else{
          cerr << "Error: The edge type is ERRORTYPE" << endl;
          // return false;
        }
        ZXFile << itr->first->getId();
      }
    }
    ZXFile << "\n";
  }
  for(size_t i=0; i<_vertices.size(); i++){
    ZXVertex* v = _vertices[i];
    if(v->getType()==VertexType::BOUNDARY){
      continue;
    }
    if(v->getType()==VertexType::Z) ZXFile << "Z";
    else if(v->getType()==VertexType::X) ZXFile << "X";
    else ZXFile << "H";
    ZXFile << v->getId();
    NeighborMap nbm = v->getNeighborMap();
    
    for(auto itr=nbm.begin(); itr!=nbm.end(); itr++){
      if(itr->first->getId() >= v->getId()){
        cout << "VV:::  "<< (itr->second) << endl;
        ZXFile << " ";
        if(*(itr->second)==EdgeType::SIMPLE)
          ZXFile << "S";
        else if(*(itr->second)==EdgeType::HADAMARD)
          ZXFile << "H";
        else{
          cerr << "Error: The edge type is ERRORTYPE" << endl;
          // return false;
        }
        ZXFile << itr->first->getId();
      }
    }
    if(v->getPhase()!=Phase(0)) ZXFile << " " << v->getPhase();
    ZXFile << "\n";
  }
  return true;
}