/****************************************************************************
  FileName     [ zxIO.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define qcir reader functions ]
  Author       [ Chin-Yi Cheng, Yi-Hsiang Kuo ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "util.h"
#include "zxGraph.h"

extern size_t verbose;

/**
 * @brief read a zx graph
 *
 * @param filename
 * @param bzx
 * @return true if correctly consturct the graph
 * @return false
 */
bool ZXGraph::readZX(string filename, bool bzx) {
    fstream ZXFile;
    ZXFile.open(filename.c_str(), ios::in);
    if (!ZXFile.is_open()) {
        cerr << "Cannot open the file \"" << filename << "\"!!" << endl;
        return false;
    }
    string line;
    string token;
    unordered_map<size_t, vector<pair<size_t, EdgeType>>> storage;
    unordered_map<size_t, ZXVertex*> vertexList;
    size_t counter = 1;
    while (getline(ZXFile, line)) {
        // Slice comment
        size_t found = line.find("//");
        if (found != string::npos)
            line = line.substr(0, found);
        if (line == "") {
            counter++;
            continue;
        }
        vector<string> tokens;
        size_t n = myStrGetTok(line, token);
        while (token.size()) {
            tokens.push_back(token);
            n = myStrGetTok(line, token, n);
        }
        unsigned id, nid, cid = 0;
        int qid;
        int column = 0;
        string vertexStr = tokens[0];
        if (vertexStr[0] == 'I' || vertexStr[0] == 'O') {
            string idStr = vertexStr.substr(1);
            if (!myStr2Uns(idStr, id)) {
                cerr << "Error: Vertex Id " << idStr << " is not an unsigned in line " << counter << "!!" << endl;
                return false;
            }
            if (storage.contains(size_t(id))) {
                cerr << "Error: Duplicated vertex Id " << size_t(id) << " in line " << counter << "!!" << endl;
                return false;
            }
            if (tokens.size() == 0) {
                cerr << "Error: Missing qubit Id after I/O declaration in line " << counter << "!!" << endl;
                return false;
            }
            if (!myStr2Int(tokens[1], qid)) {
                cerr << "Error: Qubit Id " << tokens[1] << " is not an integer in line " << counter << "!!" << endl;
                return false;
            }
            if (bzx) {
                if (!myStr2Uns(tokens[2], cid)) {
                    cerr << "Error: Column Id " << tokens[2] << " is not an unsigned in line " << counter << "!!" << endl;
                    return false;
                }
            }

            vector<pair<size_t, EdgeType>> tmp;
            for (size_t s = (bzx ? 3 : 2); s < tokens.size(); s++) {
                string neighborStr = tokens[s];
                if (neighborStr[0] == 'S' || neighborStr[0] == 'H') {
                    string nidStr = neighborStr.substr(1);
                    if (!myStr2Uns(nidStr, nid)) {
                        cerr << "Error: Vertex Id in declaring neighbor " << neighborStr << " is not an unsigned in line " << counter << "!!" << endl;
                        return false;
                    } else if (size_t(nid) >= size_t(id))
                        tmp.push_back(make_pair(size_t(nid), (neighborStr[0] == 'S') ? EdgeType::SIMPLE : EdgeType::HADAMARD));
                } else {
                    cerr << "Error: Unsupported edge type " << neighborStr[0] << " in line " << counter << "!!" << endl;
                    return false;
                }
            }
            storage[size_t(id)] = tmp;
            if (vertexStr[0] == 'I')
                vertexList[size_t(id)] = addInput(size_t(qid), true, cid);
            else
                vertexList[size_t(id)] = addOutput(size_t(qid), true, cid);
        } else if (vertexStr[0] == 'Z' || vertexStr[0] == 'X' || vertexStr[0] == 'H') {
            string idStr = vertexStr.substr(1);

            if (!myStr2Uns(idStr, id)) {
                cerr << "Error: Vertex Id " << idStr << " is not an unsigned in line " << counter << "!!" << endl;
                return false;
            }
            if (storage.contains(size_t(id))) {
                cerr << "Error: Duplicated vertex Id " << size_t(id) << " in line " << counter << "!!" << endl;
                return false;
            }
            vector<pair<size_t, EdgeType>> tmp;
            Phase ph;
            for (size_t s = (bzx ? 3 : 1); s < tokens.size(); s++) {
                string neighborStr = tokens[s];
                bool checkNeighbor = true;
                if (s == tokens.size() - 1) {
                    if (ph.fromString(neighborStr)) {
                        checkNeighbor = false;
                    }
                }
                if (checkNeighbor) {
                    if (neighborStr[0] == 'S' || neighborStr[0] == 'H') {
                        string nidStr = neighborStr.substr(1);
                        if (!myStr2Uns(nidStr, nid)) {
                            cerr << "Error: Vertex Id in declaring neighbor " << neighborStr << " is not an unsigned in line " << counter << "!!" << endl;
                            return false;
                        } else if (size_t(nid) >= size_t(id))
                            tmp.push_back(make_pair(size_t(nid), (neighborStr[0] == 'S') ? EdgeType::SIMPLE : EdgeType::HADAMARD));
                    } else if (neighborStr[0] == 'C') {
                        string colStr = neighborStr.substr(1);
                        if (!myStr2Int(colStr, column)) {
                            cerr << "Error: Column " << neighborStr << " is not an int in line " << counter << "!!" << endl;
                            return false;
                        }
                    } else {
                        cerr << "Error: Unsupported edge type " << neighborStr[0] << " in line " << counter << "!!" << endl;
                        return false;
                    }
                }
            }
            qid = 0;
            cid = 0;
            if (bzx) {
                if (!myStr2Int(tokens[1], qid)) {
                    cerr << "Error: Qubit Id " << tokens[1] << " is not an integer in line " << counter << "!!" << endl;
                    return false;
                }
                if (!myStr2Uns(tokens[2], cid)) {
                    cerr << "Error: Column Id " << tokens[2] << " is not an unsigned in line " << counter << "!!" << endl;
                    return false;
                }
            }
            storage[size_t(id)] = tmp;
            if (vertexStr[0] == 'Z')
                vertexList[size_t(id)] = addVertex(size_t(qid), VertexType::Z, ph, true, cid);
            else if (vertexStr[0] == 'X')
                vertexList[size_t(id)] = addVertex(size_t(qid), VertexType::X, ph, true, cid);
            else
                vertexList[size_t(id)] = addVertex(size_t(qid), VertexType::H_BOX, ph, true, cid);
        } else {
            cerr << "Error: Unsupported vertex type " << vertexStr[0] << " in line " << counter << "!!" << endl;
            return false;
        }

        counter++;
    }

    for (const auto [vertexId, neighbors] : storage) {
        //   for(auto itr = storage.begin(); itr!=storage.end(); itr++){
        for (const auto& [nbId, nbEdgeType] : neighbors) {
            // for(size_t nb=0; nb<neighbors.size(); nb++){
            if (vertexList[nbId] == nullptr) {
                cerr << "Found a never declared id " << nbId << " in neighbor list of vertex " << vertexId << "!!" << endl;
                return false;
            } else {
                addEdge(vertexList[vertexId], vertexList[nbId], EdgeType(nbEdgeType));
            }
        }
    }
    return true;
}

/**
 * @brief write a zxgraph
 *
 * @param filename
 * @param complete
 * @param bzx
 * @return true if correctly write a graph into .zx
 * @return false
 */
bool ZXGraph::writeZX(string filename, bool complete, bool bzx) {
    fstream ZXFile;
    ZXFile.open(filename.c_str(), std::fstream::out);
    if (!ZXFile.is_open()) {
        cerr << "Cannot open the file \"" << filename << "\"!!" << endl;
        return false;
    }
    auto writeNeighbors = [&ZXFile](ZXVertex* v, bool complete = false) {
        for (const auto& [nb, etype] : v->getNeighbors()) {
            if ((complete) || (nb->getId() >= v->getId())) {
                ZXFile << " ";
                switch (etype) {
                    case EdgeType::SIMPLE:
                        ZXFile << "S";
                        break;
                    case EdgeType::HADAMARD:
                        ZXFile << "H";
                        break;
                    default:
                        cerr << "Error: The edge type is ERRORTYPE" << endl;
                        return false;
                }
                ZXFile << nb->getId();
            }
        }
        return true;
    };
    ZXFile << "// Input \n";
    for (auto& v : _inputs) {
        ZXFile << "I" << v->getId() << " (" << v->getQubit() << "," << v->getCol() << ")";
        if (!writeNeighbors(v, complete)) return false;
        ZXFile << "\n";
    }

    ZXFile << "// Output \n";

    for (auto& v : _outputs) {
        ZXFile << "O" << v->getId() << " (" << v->getQubit() << "," << v->getCol() << ")";
        if (!writeNeighbors(v, complete)) return false;
        ZXFile << "\n";
    }

    ZXFile << "// Non-boundary \n";
    for (ZXVertex* const& v : _vertices) {
        if (v->isBoundary()) continue;

        if (v->isZ())
            ZXFile << "Z";
        else if (v->isX())
            ZXFile << "X";
        else
            ZXFile << "H";
        ZXFile << v->getId();
        if (bzx) ZXFile << " (" << v->getQubit() << "," << v->getCol() << ")";
        if (!writeNeighbors(v, complete)) return false;

        if (v->getPhase() != Phase(0)) ZXFile << " " << v->getPhase().getAsciiString();
        ZXFile << "\n";
    }
    return true;
}

string tikzStyle = "[\n"
"\t boun/.style={circle, draw=black!60, fill=black!5, very thick, text width=5mm, align=center, inner sep=0pt},\n"
"\t hbox/.style={regular polygon,regular polygon sides=4, draw=yellow!90, fill=yellow!20, very thick, text width=3.5mm, align=center, inner sep=0pt},\n"
"\t zspi/.style={circle, draw=green!60!black!100, fill=green!5, very thick, text width=5mm, align=center, inner sep=0pt},\n"
"\t xspi/.style={circle, draw=red!80, fill=red!5, very thick, text width=5mm, align=center, inner sep=0pt},\n"
"\t hedg/.style={draw=blue!100, very thick},\n"
"\t sedg/.style={draw=black, very thick},\n"
"];\n";

unordered_map<VertexType, string> vt2s = {
    {VertexType::BOUNDARY,"boun"},
    {VertexType::Z,"zspi"},
    {VertexType::X,"xspi"},
    {VertexType::H_BOX,"hbox"}
};

unordered_map<EdgeType, string> et2s = {
    {EdgeType::HADAMARD,"hedg"},
    {EdgeType::SIMPLE,"sedg"},
};

/**
 * @brief 
 * 
 * @param filename 
 * @return true 
 * @return false 
 */
bool ZXGraph::writeTikz(string filename) {
    fstream tikzFile;
    tikzFile.open(filename.c_str(), std::fstream::out);
    if (!tikzFile.is_open()) {
        cerr << "Cannot open the file \"" << filename << "\"!!" << endl;
        return false;
    }
    string fontSize = "\\tiny";
    tikzFile << "\\begin{tikzpicture}" << tikzStyle;
    tikzFile << "    % Vertices\n";

    auto writePhase = [&tikzFile, &fontSize](ZXVertex* v) {
        if(v->getPhase()==Phase(0)) 
            return true;
        tikzFile << ",label={ " << fontSize << " $";
        if(v->getPhase().getRational().denominator() == 1){
            tikzFile << to_string(v->getPhase().getRational().numerator()) << "\\pi";
        }
        else{
            tikzFile << "\\frac{" << to_string(v->getPhase().getRational().numerator()) << "\\pi}{" << to_string(v->getPhase().getRational().denominator()) << "}";
        }
        tikzFile << "$ }";
        return true;
    };

    //NOTE - Sample: \node[zspi] (88888)  at (0,1) {{\tiny 88888}};
    for (auto& v : _vertices) {
        tikzFile << "    \\node[" << vt2s[v->getType()] << writePhase(v) << "]";
        tikzFile << "(" << to_string(v->getId()) << ")  at (" << to_string(v->getCol()) << "," << to_string(v->getQubit()) << ") ";
        tikzFile << "{{" << fontSize << " " << to_string(v->getId()) << "}};\n";
    }
    //NOTE - Sample: \draw[hedg] (1234) -- (123);
    tikzFile << "    % Edges\n";
    
    for (auto& v : _vertices) {
        for (auto& [n, e] : v->getNeighbors()) {
            if (n->getId() > v->getId())
                tikzFile << "    \\draw[" << et2s[e] << "] (" << to_string(v->getId()) << ") -- (" << to_string(n->getId()) << ");\n";
        }
    }

    tikzFile << "\\end{tikzpicture}\n";
    return true;
}