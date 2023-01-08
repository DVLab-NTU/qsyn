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

string tikzStyle =
    "[\n"
    "\t boun/.style={circle, draw=black!60, fill=black!5, very thick, text width=5mm, align=center, inner sep=0pt},\n"
    "\t hbox/.style={regular polygon,regular polygon sides=4, draw=yellow!90, fill=yellow!20, very thick, text width=3.5mm, align=center, inner sep=0pt},\n"
    "\t zspi/.style={circle, draw=green!60!black!100, fill=green!5, very thick, text width=5mm, align=center, inner sep=0pt},\n"
    "\t xspi/.style={circle, draw=red!80, fill=red!5, very thick, text width=5mm, align=center, inner sep=0pt},\n"
    "\t hedg/.style={draw=blue!100, very thick},\n"
    "\t sedg/.style={draw=black, very thick},\n"
    "];\n";

unordered_map<VertexType, string> vt2s = {
    {VertexType::BOUNDARY, "boun"},
    {VertexType::Z, "zspi"},
    {VertexType::X, "xspi"},
    {VertexType::H_BOX, "hbox"}};

unordered_map<EdgeType, string> et2s = {
    {EdgeType::HADAMARD, "hedg"},
    {EdgeType::SIMPLE, "sedg"},
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
        if (v->getPhase() == Phase(0))
            return true;
        tikzFile << ",label={ " << fontSize << " $";
        if (v->getPhase().getRational().denominator() == 1) {
            tikzFile << to_string(v->getPhase().getRational().numerator()) << "\\pi";
        } else {
            tikzFile << "\\frac{" << to_string(v->getPhase().getRational().numerator()) << "\\pi}{" << to_string(v->getPhase().getRational().denominator()) << "}";
        }
        tikzFile << "$ }";
        return true;
    };

    // NOTE - Sample: \node[zspi] (88888)  at (0,1) {{\tiny 88888}};
    for (auto& v : _vertices) {
        tikzFile << "    \\node[" << vt2s[v->getType()];
        writePhase(v);
        tikzFile << "]";
        tikzFile << "(" << to_string(v->getId()) << ")  at (" << to_string(v->getCol()) << "," << to_string(v->getQubit()) << ") ";
        tikzFile << "{{" << fontSize << " " << to_string(v->getId()) << "}};\n";
    }
    // NOTE - Sample: \draw[hedg] (1234) -- (123);
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
