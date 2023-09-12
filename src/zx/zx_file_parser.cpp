/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define zxFileStructure member functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_file_parser.hpp"

#include <fstream>

#include "util/phase.hpp"
#include "util/util.hpp"

using namespace std;

/**
 * @brief Parse the file
 *
 * @param filename
 * @return true if the file is successfully parsed
 * @return false if error happens
 */
bool ZXFileParser::parse(string const& filename) {
    _storage.clear();
    _taken_input_qubits.clear();
    _taken_output_qubits.clear();

    ifstream zx_file(filename);
    if (!zx_file.is_open()) {
        cerr << "Cannot open the file \"" << filename << "\"!!" << endl;
        return false;
    }

    return _parse_internal(zx_file);
}

/**
 * @brief Parse each line in the file
 *
 * @param f
 * @return true if the file is successfully parsed
 * @return false if the format of any lines are wrong
 */
bool ZXFileParser::_parse_internal(ifstream& f) {
    // each line should be in the format of
    // <VertexString> [(<Qubit, Column>)] [NeighborString...] [Phase phase]
    _line_no = 1;
    for (string line; getline(f, line); _line_no++) {
        line = dvlab::str::strip_spaces(dvlab::str::strip_comments(line));
        if (line.empty()) continue;

        vector<string> tokens;
        if (!_tokenize(line, tokens)) return false;

        unsigned id;
        VertexInfo info;

        if (!_parse_type_and_id(tokens[0], info.type, id)) return false;

        if (info.type == 'I' || info.type == 'O') {
            if (!_is_valid_tokens_for_boundary_vertex(tokens)) return false;
        }
        if (info.type == 'H') {
            if (!_is_valid_tokens_for_h_box(tokens)) return false;
            info.phase = Phase(1);
        }

        if (!_parse_qubit(tokens[1], info.type, info.qubit)) return false;
        if (!_parse_column(tokens[2], info.column)) return false;

        Phase tmp;
        if (tokens.size() > 3) {
            if (Phase::from_string(tokens.back(), tmp)) {
                tokens.pop_back();
                info.phase = tmp;
            }

            pair<char, size_t> neighbor;
            for (size_t i = 3; i < tokens.size(); ++i) {
                if (!_parse_neighbors(tokens[i], neighbor)) return false;
                info.neighbors.emplace_back(neighbor);
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
bool ZXFileParser::_tokenize(string const& line, vector<string>& tokens) {
    string token;

    // parse first token
    size_t pos = dvlab::str::str_get_token(line, token);
    tokens.emplace_back(token);

    // parsing parenthesis
    size_t left_paren_pos = line.find_first_of('(', pos);
    size_t right_paren_pos = line.find_first_of(')', left_paren_pos == string::npos ? 0 : left_paren_pos);
    bool has_left_parenthesis = (left_paren_pos != string::npos);
    bool has_right_parenthesis = (right_paren_pos != string::npos);

    if (has_left_parenthesis) {
        if (has_right_parenthesis) {
            // coordinate string is given
            pos = dvlab::str::str_get_token(line, token, left_paren_pos + 1, ',');

            if (pos == string::npos) {
                _print_failed_at_line_no();
                cerr << "missing comma between declaration of qubit and column!!" << endl;
                return false;
            }

            token = dvlab::str::strip_spaces(token);
            if (token == "") {
                _print_failed_at_line_no();
                cerr << "missing argument before comma!!" << endl;
                return false;
            }
            tokens.emplace_back(token);

            dvlab::str::str_get_token(line, token, pos + 1, ')');

            token = dvlab::str::strip_spaces(token);
            if (token == "") {
                _print_failed_at_line_no();
                cerr << "missing argument before right parenthesis!!" << endl;
                return false;
            }
            tokens.emplace_back(token);

            pos = right_paren_pos + 1;
        } else {
            _print_failed_at_line_no();
            cerr << "missing closing parenthesis!!" << endl;
            return false;
        }
    } else {  // if no left parenthesis
        if (has_right_parenthesis) {
            _print_failed_at_line_no();
            cerr << "missing opening parenthesis!!" << endl;
            return false;
        } else {
            // coordinate info is left out
            tokens.emplace_back("-");
            tokens.emplace_back("-");
        }
    }

    // parse remaining
    pos = dvlab::str::str_get_token(line, token, pos);

    while (token.size()) {
        tokens.emplace_back(token);
        pos = dvlab::str::str_get_token(line, token, pos);
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
bool ZXFileParser::_parse_type_and_id(string const& token, char& type, unsigned& id) {
    type = toupper(token[0]);

    if (type == 'G') {
        _print_failed_at_line_no();
        cerr << "ground vertices are not supported yet!!" << endl;
        return false;
    }

    if (string("IOZXH").find(type) == string::npos) {
        _print_failed_at_line_no();
        cerr << "unsupported vertex type (" << type << ")!!" << endl;
        return false;
    }

    string id_string = token.substr(1);

    if (id_string.empty()) {
        _print_failed_at_line_no();
        cerr << "Missing vertex ID after vertex type declaration (" << type << ")!!" << endl;
        return false;
    }

    if (!dvlab::str::str_to_u(id_string, id)) {
        _print_failed_at_line_no();
        cerr << "vertex ID (" << id_string << ") is not an unsigned integer!!" << endl;
        return false;
    }

    if (_storage.contains(id)) {
        _print_failed_at_line_no();
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
bool ZXFileParser::_is_valid_tokens_for_boundary_vertex(vector<string> const& tokens) {
    if (tokens[1] == "-") {
        _print_failed_at_line_no();
        cerr << "please specify the qubit ID to boundary vertex!!" << endl;
        return false;
    }

    if (tokens.size() <= 3) return true;

    Phase tmp;
    if (Phase::from_string(tokens.back(), tmp)) {
        _print_failed_at_line_no();
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
bool ZXFileParser::_is_valid_tokens_for_h_box(vector<string> const& tokens) {
    if (tokens.size() <= 3) return true;

    Phase tmp;
    if (Phase::from_string(tokens.back(), tmp)) {
        _print_failed_at_line_no();
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
bool ZXFileParser::_parse_qubit(string const& token, char const& type, int& qubit) {
    if (token == "-") {
        qubit = 0;
        return true;
    }

    if (!dvlab::str::str_to_i(token, qubit)) {
        _print_failed_at_line_no();
        cerr << "qubit ID (" << token << ") is not an integer in line " << _line_no << "!!" << endl;
        return false;
    }

    if (type == 'I') {
        if (_taken_input_qubits.contains(qubit)) {
            _print_failed_at_line_no();
            cerr << "duplicated input qubit ID (" << qubit << ")!!" << endl;
            return false;
        }
        _taken_input_qubits.insert(qubit);
    }

    if (type == 'O') {
        if (_taken_output_qubits.contains(qubit)) {
            _print_failed_at_line_no();
            cerr << "duplicated output qubit ID (" << qubit << ")!!" << endl;
            return false;
        }
        _taken_output_qubits.insert(qubit);
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
bool ZXFileParser::_parse_column(string const& token, float& column) {
    if (token == "-") {
        column = 0;
        return true;
    }

    if (!dvlab::str::str_to_f(token, column)) {
        _print_failed_at_line_no();
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
bool ZXFileParser::_parse_neighbors(string const& token, pair<char, size_t>& neighbor) {
    char type = toupper(token[0]);
    unsigned id;
    if (string("SH").find(type) == string::npos) {
        _print_failed_at_line_no();
        cerr << "unsupported edge type (" << type << ")!!" << endl;
        return false;
    }

    string neighbor_string = token.substr(1);

    if (neighbor_string.empty()) {
        _print_failed_at_line_no();
        cerr << "Missing neighbor vertex ID after edge type declaration (" << type << ")!!" << endl;
        return false;
    }

    if (!dvlab::str::str_to_u(neighbor_string, id)) {
        _print_failed_at_line_no();
        cerr << "neighbor vertex ID (" << neighbor_string << ") is not an unsigned integer!!" << endl;
        return false;
    }

    neighbor = {type, id};
    return true;
}

/**
 * @brief Print the line failed
 *
 */
void ZXFileParser::_print_failed_at_line_no() const {
    cerr << "Error: failed to read line " << _line_no << ": ";
}