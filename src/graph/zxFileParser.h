/****************************************************************************
  FileName     [ zxFileReader.h ]
  PackageName  [ graph ]
  Synopsis     [ Define ZX format parser ]
  Author       [ Mu-Te (Joshua) Lau ]
  Copyright    [ Copyleft(c) 2022-present DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_FILE_PARSER_H
#define ZX_FILE_PARSER_H

#include <cstddef>  // for size_t
#include <iosfwd>  // for ifstream
#include <string>   // for string
#include <utility>  // for pair

#include "zxDef.h"  // for VertexInfo

class ZXFileParser {
public:
    using VertexInfo = ZXParserDetail::VertexInfo;
    using StorageType = ZXParserDetail::StorageType;

    ZXFileParser() : _lineNumber(1) {}

    bool parse(const string& filename);
    const StorageType getStorage() const { return _storage; }

private:
    unsigned _lineNumber;
    StorageType _storage;
    unordered_set<int> _takenInputQubits;
    unordered_set<int> _takenOutputQubits;

    bool parseInternal(ifstream& f);

    // parsing subroutines

    string stripLeadingSpacesAndComments(string& line);
    bool tokenize(const string& line, vector<string>& tokens);

    bool parseTypeAndId(const string& token, char& type, unsigned& id);
    bool validTokensForBoundaryVertex(const vector<string>& tokens);

    bool parseQubit(const string& token, const char& type, int& qubit);
    bool parseColumn(const string& token, float& column);

    bool parseNeighbor(const string& token, pair<char, size_t>& neighbor);

    void printFailedAtLineNum() const;
};

#endif  // ZX_FILE_PARSER_H