/****************************************************************************
  FileName     [ zxFileParser.cpp ]
  PackageName  [ graph ]
  Synopsis     [ Define zxFileStructure member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "zxFileParser.h"

#include <fstream>   // for ifstream
#include <iostream>  // for ifstream

#include "phase.h"  // for Phase
#include "util.h"   // for myStr2Float, myStr2Int

using namespace std;

/**
 * @brief Parse the file
 *
 * @param filename
 * @return true if the file is successfully parsed
 * @return false if error happens
 */
bool ZXFileParser::parse(const string& filename) {
    _storage.clear();
    _takenInputQubits.clear();
    _takenOutputQubits.clear();

    ifstream zxFile(filename);
    if (!zxFile.is_open()) {
        cerr << "Cannot open the file \"" << filename << "\"!!" << endl;
        return false;
    }

    return parseInternal(zxFile);
}

/**
 * @brief Parse each line in the file
 *
 * @param f
 * @return true if the file is successfully parsed
 * @return false if the format of any lines are wrong
 */
bool ZXFileParser::parseInternal(ifstream& f) {
    // each line should be in the format of
    // <VertexString> [(<Qubit, Column>)] [NeighborString...] [Phase phase]
    _lineNumber = 1;
    for (string line; getline(f, line); _lineNumber++) {
        line = stripWhitespaces(stripComments(line));
        if (line.empty()) continue;

        vector<string> tokens;
        if (!tokenize(line, tokens)) return false;

        unsigned id;
        VertexInfo info;

        if (!parseTypeAndId(tokens[0], info.type, id)) return false;

        if (info.type == 'I' || info.type == 'O') {
            if (!validTokensForBoundaryVertex(tokens)) return false;
        }
        if (info.type == 'H') {
            if (!validTokensForHBox(tokens)) return false;
            info.phase = Phase(1);
        }

        if (!parseQubit(tokens[1], info.type, info.qubit)) return false;
        if (!parseColumn(tokens[2], info.column)) return false;

        Phase tmp;
        if (tokens.size() > 3) {
            if (Phase::fromString(tokens.back(), tmp)) {
                tokens.pop_back();
                info.phase = tmp;
            }

            pair<char, size_t> neighbor;
            for (size_t i = 3; i < tokens.size(); ++i) {
                if (!parseNeighbor(tokens[i], neighbor)) return false;
                info.neighbors.push_back(neighbor);
            }
        }

        _storage[id] = info;
    }

    return true;
}

/**
 * @brief Tokenize the line
 *
 * @param line
 * @param tokens
 * @return true
 * @return false
 */
bool ZXFileParser::tokenize(const string& line, vector<string>& tokens) {
    string token;

    // parse first token
    size_t pos = myStrGetTok(line, token);
    tokens.push_back(token);

    // parsing parenthesis
    size_t leftParenPos = line.find_first_of("(", pos);
    size_t rightParenPos = line.find_first_of(")", leftParenPos == string::npos ? 0 : leftParenPos);
    bool hasLeftParenthesis = (leftParenPos != string::npos);
    bool hasRightParenthesis = (rightParenPos != string::npos);

    if (hasLeftParenthesis) {
        if (hasRightParenthesis) {
            // coordinate string is given
            pos = myStrGetTok(line, token, leftParenPos + 1, ',');

            if (pos == string::npos) {
                printFailedAtLineNum();
                cerr << "missing comma between declaration of qubit and column!!" << endl;
                return false;
            }

            token = stripWhitespaces(token);
            if (token == "") {
                printFailedAtLineNum();
                cerr << "missing argument before comma!!" << endl;
                return false;
            }
            tokens.push_back(token);

            pos = myStrGetTok(line, token, pos + 1, ')');

            token = stripWhitespaces(token);
            if (token == "") {
                printFailedAtLineNum();
                cerr << "missing argument before right parenthesis!!" << endl;
                return false;
            }
            tokens.push_back(token);

            pos = rightParenPos + 1;
        } else {
            printFailedAtLineNum();
            cerr << "missing closing parenthesis!!" << endl;
            return false;
        }
    } else {  // if no left parenthesis
        if (hasRightParenthesis) {
            printFailedAtLineNum();
            cerr << "missing opening parenthesis!!" << endl;
            return false;
        } else {
            // coordinate info is left out
            tokens.push_back("-");
            tokens.push_back("-");
        }
    }

    // parse remaining
    pos = myStrGetTok(line, token, pos);

    while (token.size()) {
        tokens.push_back(token);
        pos = myStrGetTok(line, token, pos);
    }

    return true;
}

/**
 * @brief Parse type and id
 *
 * @param token
 * @param type
 * @param id
 * @return true
 * @return false
 */
bool ZXFileParser::parseTypeAndId(const string& token, char& type, unsigned& id) {
    type = toupper(token[0]);

    if (type == 'G') {
        printFailedAtLineNum();
        cerr << "ground vertices are not supported yet!!" << endl;
        return false;
    }

    if (string("IOZXH").find(type) == string::npos) {
        printFailedAtLineNum();
        cerr << "unsupported vertex type (" << type << ")!!" << endl;
        return false;
    }

    string idString = token.substr(1);

    if (idString.empty()) {
        printFailedAtLineNum();
        cerr << "Missing vertex ID after vertex type declaration (" << type << ")!!" << endl;
        return false;
    }

    if (!myStr2Uns(idString, id)) {
        printFailedAtLineNum();
        cerr << "vertex ID (" << idString << ") is not an unsigned integer!!" << endl;
        return false;
    }

    if (_storage.contains(id)) {
        printFailedAtLineNum();
        cerr << "duplicated vertex ID (" << id << ")!!" << endl;
        return false;
    }

    return true;
}

/**
 * @brief Check the tokens are valid for boundary vertices
 *
 * @param tokens
 * @return true
 * @return false
 */
bool ZXFileParser::validTokensForBoundaryVertex(const vector<string>& tokens) {
    if (tokens[1] == "-") {
        printFailedAtLineNum();
        cerr << "please specify the qubit ID to boundary vertex!!" << endl;
        return false;
    }

    if (tokens.size() <= 3) return true;

    Phase tmp;
    if (Phase::fromString(tokens.back(), tmp)) {
        printFailedAtLineNum();
        cerr << "cannot assign phase to boundary vertex!!" << endl;
        return false;
    }
    return true;
}

/**
 * @brief Check the tokens are valid for H boxes
 *
 * @param tokens
 * @return true
 * @return false
 */
bool ZXFileParser::validTokensForHBox(const vector<string>& tokens) {
    if (tokens.size() <= 3) return true;

    Phase tmp;
    if (Phase::fromString(tokens.back(), tmp)) {
        printFailedAtLineNum();
        cerr << "cannot assign phase to H-box!!" << endl;
        return false;
    }
    return true;
}

/**
 * @brief Parse qubit
 *
 * @param token
 * @param type input or output
 * @param qubit will store the qubit after parsing
 * @return true
 * @return false
 */
bool ZXFileParser::parseQubit(const string& token, const char& type, int& qubit) {
    if (token == "-") {
        qubit = 0;
        return true;
    }

    if (!myStr2Int(token, qubit)) {
        printFailedAtLineNum();
        cerr << "qubit ID (" << token << ") is not an integer in line " << _lineNumber << "!!" << endl;
        return false;
    }

    if (type == 'I') {
        if (_takenInputQubits.contains(qubit)) {
            printFailedAtLineNum();
            cerr << "duplicated input qubit ID (" << qubit << ")!!" << endl;
            return false;
        }
        _takenInputQubits.insert(qubit);
    }

    if (type == 'O') {
        if (_takenOutputQubits.contains(qubit)) {
            printFailedAtLineNum();
            cerr << "duplicated output qubit ID (" << qubit << ")!!" << endl;
            return false;
        }
        _takenOutputQubits.insert(qubit);
    }

    return true;
}

/**
 * @brief Parse column
 *
 * @param token
 * @param column will store the column after parsing
 * @return true
 * @return false
 */
bool ZXFileParser::parseColumn(const string& token, float& column) {
    if (token == "-") {
        column = 0;
        return true;
    }

    if (!myStr2Float(token, column)) {
        printFailedAtLineNum();
        cerr << "column ID (" << token << ") is not an unsigned integer!!" << endl;
        return false;
    }

    return true;
}

/**
 * @brief Parser the neighbor
 *
 * @param token
 * @param neighbor will store the neighbor(s) after parsing
 * @return true
 * @return false
 */
bool ZXFileParser::parseNeighbor(const string& token, pair<char, size_t>& neighbor) {
    char type = toupper(token[0]);
    unsigned id;
    if (string("SH").find(type) == string::npos) {
        printFailedAtLineNum();
        cerr << "unsupported edge type (" << type << ")!!" << endl;
        return false;
    }

    string neighborString = token.substr(1);

    if (neighborString.empty()) {
        printFailedAtLineNum();
        cerr << "Missing neighbor vertex ID after edge type declaration (" << type << ")!!" << endl;
        return false;
    }

    if (!myStr2Uns(neighborString, id)) {
        printFailedAtLineNum();
        cerr << "neighbor vertex ID (" << neighborString << ") is not an unsigned integer!!" << endl;
        return false;
    }

    neighbor = {type, id};
    return true;
}

/**
 * @brief Print the line failed
 *
 */
void ZXFileParser::printFailedAtLineNum() const {
    cerr << "Error: failed to read line " << _lineNumber << ": ";
}