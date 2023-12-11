/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define zxFileParser structure ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <utility>

#include "./zx_def.hpp"

namespace qsyn {

namespace zx {

class ZXFileParser {
public:
    using VertexInfo  = detail::VertexInfo;
    using StorageType = detail::StorageType;

    ZXFileParser() {}

    bool parse(std::filesystem::path const& filename);
    StorageType get_storage() const { return _storage; }

private:
    unsigned _line_no = 1;
    StorageType _storage;
    std::unordered_set<int> _taken_input_qubits;
    std::unordered_set<int> _taken_output_qubits;

    bool _parse_internal(std::ifstream& f);

    // parsing subroutines
    bool _tokenize(std::string const& line, std::vector<std::string>& tokens);

    std::optional<std::pair<char, unsigned>> _parse_type_and_id(std::string const& token);
    bool _is_valid_tokens_for_boundary_vertex(std::vector<std::string> const& tokens);
    bool _is_valid_tokens_for_h_box(std::vector<std::string> const& tokens);

    bool _parse_qubit(std::string const& token, char const& type, int& qubit);
    bool _parse_column(std::string const& token, float& column);

    bool _parse_neighbors(std::string const& token, std::pair<char, size_t>& neighbor);

    void _print_failed_at_line_no() const;
};

}  // namespace zx

}  // namespace qsyn
