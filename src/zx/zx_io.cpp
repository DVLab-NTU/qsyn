/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph Reader/Writer functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/ostream.h>
#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>

#include "./zxgraph.hpp"
#include "util/sysdep.hpp"
#include "util/tmp_files.hpp"
#include "util/util.hpp"

namespace qsyn::zx {

namespace detail {

struct VertexInfo {
    char type         = 'Z';
    QubitIdType qubit = 0;
    float row         = 0.f;
    float column      = 0.f;
    std::vector<std::pair<char, size_t>> neighbors;
    Phase phase;
};

using StorageType = dvlab::utils::ordered_hashmap<size_t, VertexInfo>;

class ZXFileParser {
public:
    std::optional<StorageType> parse(std::istream& f);

    static constexpr std::string_view supported_vertex_type = "IOZXH";
    static constexpr std::string_view supported_edge_type   = "SH";

private:
    size_t _line_no = 1;
    std::unordered_set<int> _taken_input_qubits;
    std::unordered_set<int> _taken_output_qubits;

    // parsing subroutines
    bool _tokenize(std::string const& line, std::vector<std::string>& tokens);

    std::optional<std::pair<char, size_t>> _parse_type_and_id(StorageType const& storage, std::string const& token);

    bool _parse_row(std::string const& token, float& row);
    bool _parse_column(std::string const& token, float& column);

    bool _parse_neighbors(std::string const& token, std::pair<char, size_t>& neighbor);

    void _print_failed_at_line_no() const {
        spdlog::error("Error: failed to read line {}!!", _line_no);
    }
};

/**
 * @brief Parse each line in the istream
 *
 * @param f
 * @return true if the file is successfully parsed
 * @return false if the format of any lines are wrong
 */
std::optional<StorageType> ZXFileParser::parse(std::istream& f) {
    // each line should be in the format of
    // <I|O><Vertex id>   [(<Qubit, Column>)] [<<S|H><neighbor id>...] [size_t qubit_id]
    // <Z|X|H><Vertex id> [(<Qubit, Column>)] [<<S|H><neighbor id>...] [Phase phase]
    auto storage = StorageType{};
    _taken_input_qubits.clear();
    _taken_output_qubits.clear();
    _line_no = 1;

    QubitIdType max_input_qubit_id  = 0;
    QubitIdType max_output_qubit_id = 0;

    for (std::string line; std::getline(f, line); _line_no++) {
        line = dvlab::str::trim_spaces(dvlab::str::trim_comments(line));
        if (line.empty()) continue;

        std::vector<std::string> tokens;
        if (!_tokenize(line, tokens)) return std::nullopt;

        VertexInfo info;

        auto const type_and_id = _parse_type_and_id(storage, tokens[0]);
        if (!type_and_id) return std::nullopt;

        info.type = type_and_id->first;
        auto id   = type_and_id->second;

        if (info.type == 'H') info.phase = Phase(1);

        switch (info.type) {
            case 'I': {
                if (auto const qubit = dvlab::str::from_string<int>(tokens.back()); qubit.has_value() && tokens.size() > 3) {
                    tokens.pop_back();
                    info.qubit         = qubit.value();
                    max_input_qubit_id = std::max(max_input_qubit_id, info.qubit);
                } else {
                    info.qubit = max_input_qubit_id++;
                }
                if (_taken_input_qubits.contains(info.qubit)) {
                    _print_failed_at_line_no();
                    spdlog::error("duplicated input qubit ID ({})!!", info.qubit);
                    return std::nullopt;
                }
                _taken_input_qubits.insert(info.qubit);
                info.row = static_cast<float>(info.qubit);
                break;
            }
            case 'O': {
                if (auto const qubit = dvlab::str::from_string<int>(tokens.back()); qubit.has_value() && tokens.size() > 3) {
                    tokens.pop_back();
                    info.qubit          = qubit.value();
                    max_output_qubit_id = std::max(max_output_qubit_id, info.qubit);
                } else {
                    info.qubit = max_output_qubit_id++;
                }
                if (_taken_output_qubits.contains(info.qubit)) {
                    _print_failed_at_line_no();
                    spdlog::error("duplicated output qubit ID ({})!!", info.qubit);
                    return std::nullopt;
                }
                info.row = static_cast<float>(info.qubit);
                break;
            }
            default: {
                if (auto const phase = Phase::from_string(tokens.back()); phase.has_value() && tokens.size() > 3) {
                    tokens.pop_back();
                    info.phase = phase.value();
                }
                info.row = 0;
                break;
            }
        }

        if (!_parse_row(tokens[1], info.row)) return std::nullopt;
        if (!_parse_column(tokens[2], info.column)) return std::nullopt;

        std::pair<char, size_t> neighbor;
        for (size_t i = 3; i < tokens.size(); ++i) {
            if (!_parse_neighbors(tokens[i], neighbor)) return std::nullopt;
            info.neighbors.emplace_back(neighbor);
        }

        storage.emplace(id, info);
    }

    return storage;
}

/**
 * @brief Tokenize the line
 *
 * @param line
 * @param tokens
 * @return true
 * @return false
 */
bool ZXFileParser::_tokenize(std::string const& line, std::vector<std::string>& tokens) {
    std::string token;

    // parse first token
    size_t pos = dvlab::str::str_get_token(line, token);
    tokens.emplace_back(token);

    // parsing parenthesis

    enum struct ParenthesisCase {
        none,
        both,
        left,
        right
    };

    auto const left_paren_pos  = line.find_first_of('(', pos);
    auto const right_paren_pos = line.find_first_of(')', left_paren_pos == std::string::npos ? 0 : left_paren_pos);

    auto const parenthesis_case = std::invoke([&]() -> ParenthesisCase {
        auto const has_left_parenthesis  = (left_paren_pos != std::string::npos);
        auto const has_right_parenthesis = (right_paren_pos != std::string::npos);
        if (has_left_parenthesis && has_right_parenthesis) return ParenthesisCase::both;
        if (has_left_parenthesis && !has_right_parenthesis) return ParenthesisCase::left;
        if (!has_left_parenthesis && has_right_parenthesis) return ParenthesisCase::right;
        return ParenthesisCase::none;
    });

    switch (parenthesis_case) {
        case ParenthesisCase::none:
            // coordinate info is left out
            tokens.emplace_back("-");
            tokens.emplace_back("-");
            break;
        case ParenthesisCase::left:
            _print_failed_at_line_no();
            spdlog::error("missing closing parenthesis!!");
            return false;
            break;
        case ParenthesisCase::right:
            _print_failed_at_line_no();
            spdlog::error("missing opening parenthesis!!");
            return false;
            break;
        case ParenthesisCase::both:
            pos = dvlab::str::str_get_token(line, token, left_paren_pos + 1, ',');

            if (pos == std::string::npos) {
                _print_failed_at_line_no();
                spdlog::error("missing comma between declaration of qubit and column!!");
                return false;
            }

            token = dvlab::str::trim_spaces(token);
            if (token.empty()) {
                _print_failed_at_line_no();
                spdlog::error("missing argument before comma!!");
                return false;
            }
            tokens.emplace_back(token);

            dvlab::str::str_get_token(line, token, pos + 1, ')');

            token = dvlab::str::trim_spaces(token);
            if (token.empty()) {
                _print_failed_at_line_no();
                spdlog::error("missing argument before right parenthesis!!");
                return false;
            }
            tokens.emplace_back(token);

            pos = right_paren_pos + 1;
            break;
    }

    // parse remaining
    pos = dvlab::str::str_get_token(line, token, pos);

    while (!token.empty()) {
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
std::optional<std::pair<char, size_t>> ZXFileParser::_parse_type_and_id(StorageType const& storage, std::string const& token) {
    auto type = dvlab::str::toupper(token[0]);

    if (type == 'G') {
        _print_failed_at_line_no();
        spdlog::error("ground vertices are not supported yet!!");
        return std::nullopt;
    }

    if (supported_vertex_type.find(type) == std::string::npos) {
        _print_failed_at_line_no();
        spdlog::error("unsupported vertex type ({})!!", type);
        return std::nullopt;
    }

    auto const id_string = token.substr(1);

    if (id_string.empty()) {
        _print_failed_at_line_no();
        spdlog::error("Missing vertex ID after vertex type declaration ({})!!", type);
        return std::nullopt;
    }

    auto id = dvlab::str::from_string<size_t>(id_string);

    if (!id) {
        _print_failed_at_line_no();
        spdlog::error("vertex ID ({}) is not an unsigned integer!!", id_string);
        return std::nullopt;
    }

    if (storage.contains(id.value())) {
        _print_failed_at_line_no();
        spdlog::error("duplicated vertex ID ({})!!", id);
        return std::nullopt;
    }

    return std::make_optional<std::pair<char, size_t>>({type, id.value()});
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
bool ZXFileParser::_parse_row(std::string const& token, float& row) {
    if (token == "-") {
        return true;
    }

    if (!dvlab::str::str_to_f(token, row)) {
        _print_failed_at_line_no();
        spdlog::error("row ({}) is not an floating-point number!!", token);
        return false;
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
bool ZXFileParser::_parse_column(std::string const& token, float& column) {
    if (token == "-") {
        column = 0;
        return true;
    }

    if (!dvlab::str::str_to_f(token, column)) {
        _print_failed_at_line_no();
        spdlog::error("column ({}) is not an floating-point number!!", token);
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
bool ZXFileParser::_parse_neighbors(std::string const& token, std::pair<char, size_t>& neighbor) {
    auto const type = dvlab::str::toupper(token[0]);
    unsigned id     = 0;
    if (supported_edge_type.find(type) == std::string::npos) {
        _print_failed_at_line_no();
        spdlog::error("unsupported edge type ({})!!", type);
        return false;
    }

    auto const neighbor_string = token.substr(1);

    if (neighbor_string.empty()) {
        _print_failed_at_line_no();
        spdlog::error("Missing neighbor vertex ID after edge type declaration ({})!!", type);
        return false;
    }

    if (!dvlab::str::str_to_u(neighbor_string, id)) {
        _print_failed_at_line_no();
        spdlog::error("neighbor vertex ID ({}) is not an unsigned integer!!", neighbor_string);
        return false;
    }

    neighbor = {type, id};
    return true;
}

std::optional<ZXGraph> build_graph_from_parser_storage(StorageType const& storage, bool keep_id) {
    ZXGraph graph;
    std::unordered_map<size_t, ZXVertex*> id2_vertex;

    for (auto& [id, info] : storage) {
        ZXVertex* v = std::invoke(
            // clang++ does not support structured binding capture by reference with OpenMP
            [&info = info, &graph]() {
                switch (info.type) {
                    case 'I':
                        return graph.add_input(info.qubit, info.row, info.column);
                    case 'O':
                        return graph.add_output(info.qubit, info.row, info.column);
                    case 'Z':
                        return graph.add_vertex(VertexType::z, info.phase, info.row, info.column);
                    case 'X':
                        return graph.add_vertex(VertexType::x, info.phase, info.row, info.column);
                    case 'H':
                        return graph.add_vertex(VertexType::h_box, info.phase, info.row, info.column);
                    default:
                        DVLAB_UNREACHABLE("unsupported vertex type");
                }
            });

        if (keep_id) v->set_id(id);
        id2_vertex[id] = v;
    }

    for (auto& [vid, info] : storage) {
        for (auto& [type, nbid] : info.neighbors) {
            if (!id2_vertex.contains(nbid)) {
                spdlog::error("failed to build the graph: cannot find vertex with ID {}!!", nbid);
                return std::nullopt;
            }
            auto const etype = (type == 'S') ? EdgeType::simple : EdgeType::hadamard;
            if (graph.is_neighbor(id2_vertex[vid], id2_vertex[nbid], etype)) continue;
            graph.add_edge(id2_vertex[vid], id2_vertex[nbid], etype);
        }
    }
    return graph;
}

}  // namespace detail
/**
 * @brief Read a ZXGraph
 *
 * @param filename
 * @param keepID if true, keep the IDs as written in file; if false, rearrange the vertex IDs
 * @return true if correctly constructed the graph
 * @return false
 */
std::optional<ZXGraph> from_zx(std::filesystem::path const& filepath, bool keep_id) {
    std::ifstream zx_file(filepath);

    if (!zx_file.is_open()) {
        spdlog::error("Cannot open the file \"{}\"!!", filepath);
        return std::nullopt;
    }

    auto const storage = detail::ZXFileParser{}.parse(zx_file);

    if (!storage) {
        spdlog::error("failed to parse the file \"{}\"!!", filepath.string());
        return std::nullopt;
    }

    return build_graph_from_parser_storage(storage.value(), keep_id);
}

std::optional<ZXGraph> from_zx(std::istream& istr, bool keep_id) {
    auto const storage = detail::ZXFileParser{}.parse(istr);

    if (!storage) {
        spdlog::error("failed to parse the input stream!!");
        return std::nullopt;
    }

    return build_graph_from_parser_storage(storage.value(), keep_id);
}

/**
 * @brief Write a ZXGraph
 *
 * @param filename
 * @param complete
 * @return true if correctly write a graph into .zx
 * @return false
 */
bool ZXGraph::write_zx(std::filesystem::path const& filename, bool complete) const {
    std::ofstream zx_file;
    zx_file.open(filename);
    if (!zx_file.is_open()) {
        spdlog::error("Cannot open the file \"{}\"!!", filename.string());
        return false;
    }

    auto write_neighbors = [&zx_file, complete, this](ZXVertex* v) {
        for (const auto& [nb, etype] : this->get_neighbors(v)) {
            if ((complete) || (nb->get_id() >= v->get_id())) {
                zx_file << " ";
                switch (etype) {
                    case EdgeType::simple:
                        zx_file << "S";
                        break;
                    case EdgeType::hadamard:
                    default:
                        zx_file << "H";
                        break;
                }
                zx_file << nb->get_id();
            }
        }
        return true;
    };
    fmt::println(zx_file, "// Generated by qsyn " QSYN_VERSION ", DVLab, NTUEE");
    fmt::println(zx_file, "// inputs");

    for (ZXVertex* v : get_inputs()) {
        fmt::print(zx_file, "I{} ({}, {})", v->get_id(), v->get_qubit(), std::floor(v->get_col()));
        if (!write_neighbors(v)) {
            spdlog::error("failed to write neighbors for vertex {}", v->get_id());
            return false;
        }
        fmt::println(zx_file, "");
    }

    fmt::println(zx_file, "// outputs");

    for (ZXVertex* v : get_outputs()) {
        fmt::print(zx_file, "O{} ({}, {})", v->get_id(), v->get_qubit(), std::floor(v->get_col()));
        if (!write_neighbors(v)) {
            spdlog::error("failed to write neighbors for vertex {}", v->get_id());
            return false;
        }
        fmt::println(zx_file, "");
    }

    fmt::println(zx_file, "// non-boundary vertices");

    for (ZXVertex* v : _vertices) {
        if (v->is_boundary()) continue;
        fmt::print(zx_file, "{}{} ({}, {})",
                   v->is_z()   ? "Z"
                   : v->is_x() ? "X"
                               : "H",
                   v->get_id(),
                   v->get_row(),
                   std::floor(v->get_col()));

        if (!write_neighbors(v)) {
            spdlog::error("failed to write neighbors for vertex {}", v->get_id());
            return false;
        }

        if (v->get_phase() != (v->is_hbox() ? Phase(1) : Phase(0))) {
            fmt::print(zx_file, " {}", v->get_phase().get_ascii_string());
        }
        fmt::println(zx_file, "");
    }
    return true;
}

/**
 * @brief Generate tikz file
 *
 * @param filename
 * @return true if the filename is valid
 * @return false if not
 */
bool ZXGraph::write_tikz(std::string const& filename) const {
    std::ofstream tikz_file{filename};
    if (!tikz_file.is_open()) {
        spdlog::error("Cannot open the file \"{}\"!!", filename);
        return false;
    }

    return write_tikz(tikz_file);
}

/**
 * @brief write tikz file to the fstream `tikzFile`
 *
 * @param tikzFile
 * @return true if the filename is valid
 * @return false if not
 */
bool ZXGraph::write_tikz(std::ostream& os) const {
    static std::unordered_map<VertexType, std::string> const vt2s = {
        {VertexType::boundary, "bnd"},
        {VertexType::z, "zsp"},
        {VertexType::x, "xsp"},
        {VertexType::h_box, "hbx"}};

    static std::unordered_map<EdgeType, std::string> const et2s = {
        {EdgeType::hadamard, "hedge"},
        {EdgeType::simple, "sedge"}};

    static constexpr std::string_view font_size = "tiny";

    // REVIEW - add scale
    // auto max_col = gsl::narrow_cast<int>(std::max(
    //     std::ranges::max(_inputs | std::views::transform([](ZXVertex* v) { return v->get_col(); })),
    //     std::ranges::max(_outputs | std::views::transform([](ZXVertex* v) { return v->get_col(); }))));

    // double scale = 25. / max_col;
    // scale        = (scale > 3.0) ? 3.0 : scale;

    auto get_attr_string = [](ZXVertex* v) {
        std::string result = vt2s.at(v->get_type());
        // don't print phase for zero-phase vertices, except for h-boxes we don't print when phase is pi.
        if ((v->get_phase() == Phase(0) && !v->is_hbox()) || (v->get_phase() == Phase(1) && v->is_hbox())) {
            return result;
        }

        std::string_view label_style = "[label distance=-2]90:{\\color{phaseColor}";

        auto numerator            = v->get_phase().numerator();
        std::string numerator_str = fmt::format("{}\\pi",
                                                numerator == 1    ? ""
                                                : numerator == -1 ? "-"
                                                                  : std::to_string(numerator));

        auto sans_serif_styled = [](auto const& val) { return fmt::format("\\mathsf{{{}}}", val); };

        auto denominator         = v->get_phase().denominator();
        std::string fraction_str = std::invoke([&]() -> std::string {
            if (denominator == 1) {
                return sans_serif_styled(numerator_str);
            } else {
                return fmt::format("\\frac{{{}}}{{{}}}", sans_serif_styled(numerator_str), sans_serif_styled(denominator));
            }
        });

        fmt::format_to(std::back_inserter(result), ", label={{{0} \\{1} ${2}$}}}}", label_style, font_size, fraction_str);
        return result;
    };

    fmt::println(os, "% Generated by qsyn " QSYN_VERSION ", DVLab, NTUEE");

    // color definition
    fmt::println(os, "\\definecolor{{zx_red}}{{RGB}}{{253, 160, 162}}");
    fmt::println(os, "\\definecolor{{zx_green}}{{RGB}}{{206, 254, 206}}");
    fmt::println(os, "\\definecolor{{hedgeColor}}{{RGB}}{{40, 160, 240}}");
    fmt::println(os, "\\definecolor{{phaseColor}}{{RGB}}{{14, 39, 100}}");
    fmt::println(os, "");
    // the main tikzpicture
    // REVIEW - add scale and replace 1
    fmt::println(os, "\\scalebox{{{}}}{{", 1);
    fmt::println(os, "    \\begin{{tikzpicture}}[");
    // node and edge styles
    fmt::println(os, "        font = \\sffamily,");
    fmt::println(os, "        yscale=-1,");
    fmt::println(os, "        boun/.style={{circle, text=yellow!60, font=\\sffamily, draw=black!100, fill=black!60, thick, text width=3mm, align=center, inner sep=0pt}},");
    fmt::println(os, "        hbox/.style={{regular polygon, regular polygon sides=4, font=\\sffamily, draw=yellow!40!black!100, fill=yellow!40, text width=2.5mm, align=center, inner sep=0pt}},");
    fmt::println(os, "        zspi/.style={{circle, font=\\sffamily, draw=green!60!black!100, fill=zx_green, text width=5mm, align=center, inner sep=0pt}},");
    fmt::println(os, "        xspi/.style={{circle, font=\\sffamily, draw=red!60!black!100, fill=zx_red, text width=5mm, align=center, inner sep=0pt}},");
    fmt::println(os, "        hedg/.style={{draw=hedgeColor, thick}},");
    fmt::println(os, "        sedg/.style={{draw=black, thick}},");
    fmt::println(os, "    ];");
    // content of the tikzpicture
    fmt::println(os, "        % vertices");
    // drawing vertices: \node[zspi] (88888)  at (0,1) {{\tiny 88888}};
    for (auto& v : get_vertices()) {
        fmt::println(os, "        \\node[{0}]({1})  at ({2}, {3}) {{{{\\{4} {1}}}}};", get_attr_string(v), v->get_id(), v->get_col(), v->get_row(), font_size);
    }  // end for vertices
    fmt::println(os, "");
    fmt::println(os, "        % edges");
    // drawing edges: \draw[hedg] (1234) -- (123);
    for (auto& v : _vertices) {
        for (auto& [n, e] : this->get_neighbors(v)) {
            if (n->get_id() > v->get_id()) {
                if (n->get_col() == v->get_col() && n->get_row() == v->get_row()) {
                    spdlog::warn("{} and {} are connected but they have same coordinates.", v->get_id(), n->get_id());
                    fmt::println(os, "        % \\draw[{0}] ({1}) -- ({2});", et2s.at(e), v->get_id(), n->get_id());
                } else {
                    fmt::println(os, "        \\draw[{0}] ({1}) -- ({2});", et2s.at(e), v->get_id(), n->get_id());
                }
            }
        }
    }
    fmt::println(os, "    \\end{{tikzpicture}}");
    fmt::println(os, "}}");
    return true;
}

/**
 * @brief Generate pdf file
 *
 * @param filename
 * @param toPDF if true, compile it to .pdf
 * @return true
 * @return false
 */
bool ZXGraph::write_pdf(std::string const& filename) const {
    if (!dvlab::utils::pdflatex_exists()) {
        spdlog::error("Unable to locate 'pdflatex' on your system. Please ensure that it is installed and in your system's PATH.");
        return false;
    }

    namespace fs = std::filesystem;
    namespace dv = dvlab::utils;
    fs::path filepath{filename};

    if (filepath.extension() == "") {
        spdlog::error("no file extension!!");
        return false;
    }

    if (filepath.extension() != ".pdf") {
        spdlog::error("unsupported file extension \"{}\"!!", filepath.extension().string());
        return false;
    }

    filepath.replace_extension(".tex");
    if (filepath.parent_path().empty()) {
        filepath = "./" + filepath.string();
    }

    std::error_code ec;
    fs::create_directory(filepath.parent_path(), ec);
    if (ec) {
        spdlog::error("failed to create the directory");
        spdlog::error("{}", ec.message());
        return false;
    }

    dv::TmpDir const tmp_dir;

    auto temp_tex_path = tmp_dir.path() / filepath.filename();

    std::ofstream tex_file{temp_tex_path};
    if (!tex_file.is_open()) {
        spdlog::error("Cannot open the file \"{}\"!!", filepath.string());
        return false;
    }

    if (!write_tex(tex_file)) return false;

    tex_file.close();

    // Unix cmd: pdflatex -halt-on-error -output-directory <path/to/dir> <path/to/tex>
    auto const cmd = fmt::format("pdflatex -halt-on-error -output-directory {0} {1} >/dev/null 2>&1", temp_tex_path.parent_path().string(), temp_tex_path.string());
    if (system(cmd.c_str()) != 0) {
        spdlog::error("failed to generate PDF");
        return false;
    }

    filepath.replace_extension(".pdf");

    if (fs::exists(filepath))
        fs::remove(filepath);

    // NOTE - copy instead of rename to avoid cross device link error
    fs::copy(temp_tex_path.replace_extension(".pdf"), filepath);

    return true;
}

/**
 * @brief Generate pdf file
 *
 * @param filename
 * @param toPDF if true, compile it to .pdf
 * @return true
 * @return false
 */
bool ZXGraph::write_tex(std::string const& filename) const {
    namespace fs = std::filesystem;
    fs::path const filepath{filename};

    if (filepath.extension() == "") {
        spdlog::error("no file extension!!");
        return false;
    }

    if (filepath.extension() != ".tex") {
        spdlog::error("unsupported file extension \"{}\"!!", filepath.extension().string());
        return false;
    }

    if (!filepath.parent_path().empty()) {
        std::error_code ec;
        fs::create_directory(filepath.parent_path(), ec);
        if (ec) {
            spdlog::error("failed to create the directory");
            spdlog::error("{}", ec.message());
            return false;
        }
    }

    std::ofstream tex_file{filepath};
    if (!tex_file.is_open()) {
        spdlog::error("Cannot open the file \"{}\"!!", filepath.string());
        return false;
    }

    return write_tex(tex_file);
}

/**
 * @brief Generate tex file
 *
 * @param filename
 * @return true if the filename is valid
 * @return false if not
 */
bool ZXGraph::write_tex(std::ostream& os) const {
    constexpr std::string_view includes =
        "\\documentclass[preview,border=2px]{standalone}\n"
        "\\usepackage[english]{babel}\n"
        "\\usepackage[top=2cm,bottom=2cm,left=1cm,right=1cm,marginparwidth=1.75cm]{geometry}"
        "\\usepackage{amsmath}\n"
        "\\usepackage{tikz}\n"
        "\\usetikzlibrary{shapes}\n"
        "\\usetikzlibrary{plotmarks}\n"
        "\\usepackage[colorlinks=true, allcolors=blue]{hyperref}\n"
        "\\usetikzlibrary{positioning}\n"
        "\\usetikzlibrary{shapes.geometric}\n";

    fmt::println(os, "{}", includes);
    fmt::println(os, "\\begin{{document}}\n");
    if (!write_tikz(os)) {
        spdlog::error("Failed to write tikz");
        return false;
    }
    fmt::println(os, "\\end{{document}}\n");
    return true;
}

}  // namespace qsyn::zx
