/****************************************************************************
  FileName     [ zxFileReader.h ]
  PackageName  [ graph ]
  Synopsis     [ Define zxFileParser structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#ifndef ZX_FILE_PARSER_H
#define ZX_FILE_PARSER_H

#include <cstddef>  // for size_t
#include <iosfwd>   // for ifstream
#include <string>   // for string
#include <utility>  // for pair

#include "zxDef.h"  // for VertexInfo

class ZXFileParser {
public:
    using VertexInfo = ZXParserDetail::VertexInfo;
    using StorageType = ZXParserDetail::StorageType;

    ZXFileParser() : _lineNumber(1) {}

    bool parse(const std::string& filename);
    const StorageType getStorage() const { return _storage; }

private:
    unsigned _lineNumber;
    StorageType _storage;
    std::unordered_set<int> _takenInputQubits;
    std::unordered_set<int> _takenOutputQubits;

    bool parseInternal(std::ifstream& f);

    // parsing subroutines

    std::string stripLeadingSpacesAndComments(std::string& line);
    bool tokenize(const std::string& line, std::vector<std::string>& tokens);

    bool parseTypeAndId(const std::string& token, char& type, unsigned& id);
    bool validTokensForBoundaryVertex(const std::vector<std::string>& tokens);
    bool validTokensForHBox(const std::vector<std::string>& tokens);

    bool parseQubit(const std::string& token, const char& type, int& qubit);
    bool parseColumn(const std::string& token, float& column);

    bool parseNeighbor(const std::string& token, std::pair<char, size_t>& neighbor);

    void printFailedAtLineNum() const;
};

#endif  // ZX_FILE_PARSER_H