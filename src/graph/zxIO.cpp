/****************************************************************************
  FileName     [ zxIO.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define class ZXGraph Reader/Writer functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>  // for assert
#include <cmath>
#include <cstddef>  // for size_t
#include <fstream>
#include <iostream>
#include <string>

#include "zxFileParser.h"
#include "zxGraph.h"

extern size_t verbose;

using namespace std;

/**
 * @brief Read a ZX-graph
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
            cerr << "Error: unsupported file extension \"" << extensionString << "\"!!" << endl;
            return false;
        }
    }

    ZXFileParser parser;
    string lastname = filename.substr(filename.find_last_of('/') + 1);
    setFileName(lastname.substr(0, lastname.size() - 3));
    _procedures.clear();
    return parser.parse(filename) && buildGraphFromParserStorage(parser.getStorage());
}

/**
 * @brief Write a ZX-graph
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
        ZXFile << "I" << v->getId() << " (" << v->getQubit() << "," << floor(v->getCol()) << ")";
        if (!writeNeighbors(v)) return false;
        ZXFile << "\n";
    }

    ZXFile << "// Output \n";

    for (auto& v : _outputs) {
        ZXFile << "O" << v->getId() << " (" << v->getQubit() << "," << floor(v->getCol()) << ")";
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

        ZXFile << " (" << v->getQubit() << "," << floor(v->getCol()) << ")";  // NOTE - always output coordinate now
        if (!writeNeighbors(v)) return false;

        if (v->getPhase() != (v->isHBox() ? Phase(1) : Phase(0))) ZXFile << " " << v->getPhase().getAsciiString();
        ZXFile << "\n";
    }
    return true;
}

/**
 * @brief Build graph from parser storage
 *
 * @param storage
 * @param keepID
 * @return true
 * @return false
 */
bool ZXGraph::buildGraphFromParserStorage(const ZXParserDetail::StorageType& storage, bool keepID) {
    unordered_map<size_t, ZXVertex*> id2Vertex;

    for (auto& [id, info] : storage) {
        ZXVertex* v = std::invoke(
            [&id, &info, this]() {
                if (info.type == 'I')
                    return addInput(info.qubit, true, info.column);
                if (info.type == 'O')
                    return addOutput(info.qubit, true, info.column);
                VertexType vtype;
                if (info.type == 'Z')
                    vtype = VertexType::Z;
                else if (info.type == 'X')
                    vtype = VertexType::X;
                else
                    vtype = VertexType::H_BOX;
                return addVertex(info.qubit, vtype, info.phase, true, info.column);
            });

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

string defineColors =
    "\\definecolor{zx_red}{RGB}{253, 160, 162}\n"
    "\\definecolor{zx_green}{RGB}{206, 254, 206}\n"
    "\\definecolor{hedgeColor}{RGB}{40, 160, 240}\n"
    "\\definecolor{phaseColor}{RGB}{14, 39, 100}\n";

string tikzStyle =
    "[\n"
    "font = \\sffamily,\n"
    "\t yscale=-1,\n"
    "\t boun/.style={circle, text=yellow!60, font=\\sffamily, draw=black!100, fill=black!60, thick, text width=3mm, align=center, inner sep=0pt},\n"
    "\t hbox/.style={regular polygon, regular polygon sides=4, font=\\sffamily, draw=yellow!40!black!100, fill=yellow!40, text width=2.5mm, align=center, inner sep=0pt},\n"
    "\t zspi/.style={circle, font=\\sffamily, draw=green!60!black!100, fill=zx_green, text width=5mm, align=center, inner sep=0pt},\n"
    "\t xspi/.style={circle, font=\\sffamily, draw=red!60!black!100, fill=zx_red, text width=5mm, align=center, inner sep=0pt},\n"
    "\t hedg/.style={draw=hedgeColor, thick},\n"
    "\t sedg/.style={draw=black, thick},\n"
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
 * @brief Generate tikz file
 *
 * @param filename
 * @return true if the filename is valid
 * @return false if not
 */
bool ZXGraph::writeTikz(string filename) {
    fstream tikzFile;
    tikzFile.open(filename.c_str(), std::fstream::app | fstream::out);
    if (!tikzFile.is_open()) {
        cerr << "Cannot open the file \"" << filename << "\"!!" << endl;
        return false;
    }
    string fontSize = "\\tiny";

    size_t max = 0;
    for (auto& v : _outputs) {
        if (max < v->getCol())
            max = v->getCol();
    }
    for (auto& v : _inputs) {
        if (max < v->getCol())
            max = v->getCol();
    }
    double scale = (double)25 / (double)static_cast<int>(max);
    scale = (scale > 3.0) ? 3.0 : scale;
    tikzFile << defineColors;
    tikzFile << "\\scalebox{" << to_string(scale) << "}{";
    tikzFile << "\\begin{tikzpicture}" << tikzStyle;
    tikzFile << "    % Vertices\n";

    auto writePhase = [&tikzFile, &fontSize](ZXVertex* v) {
        if (v->getPhase() == Phase(0) && v->getType() != VertexType::H_BOX)
            return true;
        if (v->getPhase() == Phase(1) && v->getType() == VertexType::H_BOX)
            return true;
        string labelStyle = "[label distance=-2]90:{\\color{phaseColor}";
        tikzFile << ",label={ " << labelStyle << fontSize << " $";
        int numerator = v->getPhase().numerator();
        int denominator = v->getPhase().denominator();

        if (denominator != 1) {
            tikzFile << "\\frac{";
        }
        if (numerator != 1) {
            tikzFile << "\\mathsf{" << to_string(numerator) << "}";
        }
        tikzFile << "\\pi";
        if (denominator != 1) {
            tikzFile << "}{ \\mathsf{" << to_string(denominator) << "}}";
        }
        tikzFile << "$ }}";
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
            if (n->getId() > v->getId()) {
                if (n->getCol() == v->getCol() && n->getQubit() == v->getQubit()) {
                    cerr << "Warning: " << v->getId() << " and " << n->getId() << " are connected but they have same coordinates." << endl;
                    tikzFile << "    % \\draw[" << et2s[e] << "] (" << to_string(v->getId()) << ") -- (" << to_string(n->getId()) << ");\n";
                } else
                    tikzFile << "    \\draw[" << et2s[e] << "] (" << to_string(v->getId()) << ") -- (" << to_string(n->getId()) << ");\n";
            }
        }
    }

    tikzFile << "\\end{tikzpicture}}\n";
    return true;
}

/**
 * @brief Write .tex file
 *
 * @param filename
 * @param toPDF if true, compile it to .pdf
 * @return true
 * @return false
 */
bool ZXGraph::writeTex(string filename, bool toPDF) {
    size_t extensionPosition = filename.find_last_of(".");
    if (extensionPosition != string::npos) {
        string extensionString = filename.substr(extensionPosition);
        if (
            myStrNCmp(".tex", extensionString, 4) != 0 &&
            myStrNCmp(".pdf", extensionString, 4) != 0) {  // backward compatibility
            cerr << "Error: unsupported file extension \"" << extensionString << "\"!!" << endl;
            return false;
        }
    } else {
        cerr << "Error: no file extension!!" << endl;
        return false;
    }

    size_t directoryPosition = filename.find_last_of("/");
    string directory = "./";
    if (directoryPosition != string::npos) {
        directory += filename.substr(0, directoryPosition);
    }
    string cmd = "mkdir -p " + directory;
    int systemRet = system(cmd.c_str());
    if (systemRet == -1) {
        cerr << "Error: fail to open the directory" << endl;
        return false;
    }
    string name = filename.substr(0, extensionPosition);
    filename = name + ".tex";
    fstream texFile;
    texFile.open(filename.c_str(), fstream::out);
    if (!texFile.is_open()) {
        cerr << "Error: cannot open the file \"" << filename << "\"!!" << endl;
        return false;
    }

    string includes =
        "\\documentclass[a4paper,landscape]{article}\n"
        "\\usepackage[english]{babel}\n"
        "\\usepackage[top=2cm,bottom=2cm,left=1cm,right=1cm,marginparwidth=1.75cm]{geometry}"
        "\\usepackage{amsmath}\n"
        "\\usepackage{tikz}\n"
        "\\usetikzlibrary{shapes}\n"
        "\\usetikzlibrary{plotmarks}\n"
        "\\usepackage[colorlinks=true, allcolors=blue]{hyperref}\n"
        "\\usetikzlibrary{positioning}\n"
        "\\usetikzlibrary{shapes.geometric}\n";
    texFile << includes;
    texFile << "\\begin{document}\n";
    texFile.flush();
    if (!writeTikz(filename)) {
        cout << "Failed" << endl;
        return false;
    }
    texFile.close();
    texFile.open(filename.c_str(), fstream::app | fstream::out);
    texFile << "\\end{document}\n";
    texFile.flush();
    texFile.close();
    if (toPDF) {
        // NOTE - Linux cmd: pdflatex -halt-on-error -output-directory <path/to/dir> <path/to/tex>
        cmd = "pdflatex -halt-on-error -output-directory " + directory + " " + filename + " >/dev/null 2>&1 ";
        systemRet = system(cmd.c_str());
        if (systemRet == -1) {
            cerr << "Error: fail to generate PDF" << endl;
            return false;
        }

        // NOTE - Clean up

        string extensions[4] = {".aux", ".log", ".out", ".tex"};
        for (auto& ext : extensions) {
            cmd = "rm " + name + ext;
            systemRet = system(cmd.c_str());
            if (systemRet == -1) {
                cerr << "Error: fail to remove compiling files." << endl;
                return false;
            }
        }
    }
    return true;
}
