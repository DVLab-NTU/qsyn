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
#include "zxFileParser.h"
#include "zxGraph.h"

extern size_t verbose;

/**
 * @brief read a zx graph
 *
 * @param filename
 * @param keepID if true, keep the IDs as written in file; if false, rearrange the vertex IDs
 * @return true if correctly constructed the graph
 * @return false
 */
bool ZXGraph::readZX(const string& filename, bool keepID) {
    size_t extensionPosition = filename.find_last_of(".");
    // REVIEW - should we guard the case of no file extension?
    if (extensionPosition != string::npos) {
        string extensionString = filename.substr(extensionPosition);
        if (
            myStrNCmp(".zx", extensionString, 3) != 0 &&
            myStrNCmp(".bzx", extensionString, 4) != 0) {  // backward compatibility
            cerr << "Unsupported file extension \"" << extensionString << "\"!!" << endl;
            return false;
        }
    }

    ZXFileParser parser;

    return parser.parse(filename) && buildGraphFromParserStorage(parser.getStorage());
}

/**
 * @brief write a zxgraph
 *
 * @param filename
 * @param complete
 * @return true if correctly write a graph into .zx
 * @return false
 */
bool ZXGraph::writeZX(const string& filename, bool complete) {
    ofstream ZXFile;
    ZXFile.open(filename);
    if (!ZXFile.is_open()) {
        cerr << "Cannot open the file \"" << filename << "\"!!" << endl;
        return false;
    }
    auto writeNeighbors = [&ZXFile, complete](ZXVertex* v) {
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
        if (!writeNeighbors(v)) return false;
        ZXFile << "\n";
    }

    ZXFile << "// Output \n";

    for (auto& v : _outputs) {
        ZXFile << "O" << v->getId() << " (" << v->getQubit() << "," << v->getCol() << ")";
        if (!writeNeighbors(v)) return false;
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

        ZXFile << " (" << v->getQubit() << "," << v->getCol() << ")";  // NOTE - always output coordinate now
        if (!writeNeighbors(v)) return false;

        if (v->getPhase() != Phase(0)) ZXFile << " " << v->getPhase().getAsciiString();
        ZXFile << "\n";
    }
    return true;
}

bool ZXGraph::buildGraphFromParserStorage(const ZXParserDetail::StorageType& storage, bool keepID) {
    unordered_map<size_t, ZXVertex*> id2Vertex;

    for (auto& [id, info] : storage) {
        ZXVertex* v;

        if (info.type == 'I')
            v = addInput(info.qubit, true, info.column);
        else if (info.type == 'O')
            v = addOutput(info.qubit, true, info.column);
        else {
            VertexType vtype;
            if (info.type == 'Z')
                vtype = VertexType::Z;
            else if (info.type == 'X')
                vtype = VertexType::X;
            else
                vtype = VertexType::H_BOX;
            v = addVertex(info.qubit, vtype, info.phase, true, info.column);
        }

        assert(v != nullptr);

        if (keepID) v->setId(id);
        id2Vertex[id] = v;
    }

    for (auto& [vid, info] : storage) {
        for (auto& [type, nbid] : info.neighbors) {
            if (!id2Vertex.contains(nbid)) {
                cerr << "Error: Failed to build the graph: cannot find vertex with ID " << nbid << "!!" << endl;
                return false;
            }

            if (vid < nbid) {
                addEdge(id2Vertex[vid], id2Vertex[nbid], (type == 'S') ? EdgeType::SIMPLE : EdgeType::HADAMARD);
            }
        }
    }
    return true;
}
