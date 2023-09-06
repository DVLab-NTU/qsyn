/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define class ZXGraph Reader/Writer functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

#include "./zx_file_parser.hpp"
#include "./zxgraph.hpp"
#include "util/logger.hpp"
#include "util/tmp_files.hpp"

using namespace std;

extern dvlab::Logger LOGGER;

/**
 * @brief Read a ZXGraph
 *
 * @param filename
 * @param keepID if true, keep the IDs as written in file; if false, rearrange the vertex IDs
 * @return true if correctly constructed the graph
 * @return false
 */
bool ZXGraph::read_zx(std::filesystem::path const& filepath, bool keep_id) {
    // REVIEW - should we guard the case of no file extension?
    if (filepath.has_extension()) {
        if (filepath.extension() != ".zx" && filepath.extension() != ".bzx") {
            fmt::println("unsupported file extension \"{}\"!!", filepath.extension().string());
            return false;
        }
    }

    ZXFileParser parser;
    return parser.parse(filepath.string()) && _build_graph_from_parser_storage(parser.get_storage());
}

/**
 * @brief Write a ZXGraph
 *
 * @param filename
 * @param complete
 * @return true if correctly write a graph into .zx
 * @return false
 */
bool ZXGraph::write_zx(string const& filename, bool complete) const {
    ofstream zx_file;
    zx_file.open(filename);
    if (!zx_file.is_open()) {
        LOGGER.error("Cannot open the file \"{}\"!!", filename);
        return false;
    }
    auto write_neighbors = [&zx_file, complete](ZXVertex* v) {
        for (const auto& [nb, etype] : v->get_neighbors()) {
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
    zx_file << "// Input \n";
    for (auto& v : _inputs) {
        zx_file << "I" << v->get_id() << " (" << v->get_qubit() << "," << floor(v->get_col()) << ")";
        if (!write_neighbors(v)) return false;
        zx_file << "\n";
    }

    zx_file << "// Output \n";

    for (auto& v : _outputs) {
        zx_file << "O" << v->get_id() << " (" << v->get_qubit() << "," << floor(v->get_col()) << ")";
        if (!write_neighbors(v)) return false;
        zx_file << "\n";
    }

    zx_file << "// Non-boundary \n";
    for (ZXVertex* const& v : _vertices) {
        if (v->is_boundary()) continue;

        if (v->is_z())
            zx_file << "Z";
        else if (v->is_x())
            zx_file << "X";
        else
            zx_file << "H";
        zx_file << v->get_id();

        zx_file << " (" << v->get_qubit() << "," << floor(v->get_col()) << ")";  // NOTE - always output coordinate now
        if (!write_neighbors(v)) return false;

        if (v->get_phase() != (v->is_hbox() ? Phase(1) : Phase(0))) zx_file << " " << v->get_phase().get_ascii_string();
        zx_file << "\n";
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
bool ZXGraph::_build_graph_from_parser_storage(detail::StorageType const& storage, bool keep_id) {
    unordered_map<size_t, ZXVertex*> id2_vertex;

    for (auto& [id, info] : storage) {
        ZXVertex* v = std::invoke(
            // clang++ does not support structured binding capture by reference with OpenMP
            [&id = id, &info = info, this]() {
                if (info.type == 'I')
                    return add_input(info.qubit, info.column);
                if (info.type == 'O')
                    return add_output(info.qubit, info.column);
                VertexType vtype;
                if (info.type == 'Z')
                    vtype = VertexType::z;
                else if (info.type == 'X')
                    vtype = VertexType::x;
                else
                    vtype = VertexType::h_box;
                return add_vertex(info.qubit, vtype, info.phase, info.column);
            });

        if (keep_id) v->set_id(id);
        id2_vertex[id] = v;
    }

    for (auto& [vid, info] : storage) {
        for (auto& [type, nbid] : info.neighbors) {
            if (!id2_vertex.contains(nbid)) {
                LOGGER.error("failed to build the graph: cannot find vertex with ID {}!!", nbid);
                return false;
            }

            if (vid < nbid) {
                add_edge(id2_vertex[vid], id2_vertex[nbid], (type == 'S') ? EdgeType::simple : EdgeType::hadamard);
            }
        }
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
bool ZXGraph::write_tikz(string const& filename) const {
    fstream tikz_file{filename, ios::out};
    if (!tikz_file.is_open()) {
        LOGGER.error("Cannot open the file \"{}\"!!", filename);
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
bool ZXGraph::write_tikz(std::ostream& filename) const {
    constexpr string_view define_colors =
        "\\definecolor{zx_red}{RGB}{253, 160, 162}\n"
        "\\definecolor{zx_green}{RGB}{206, 254, 206}\n"
        "\\definecolor{hedgeColor}{RGB}{40, 160, 240}\n"
        "\\definecolor{phaseColor}{RGB}{14, 39, 100}\n";

    constexpr string_view tikz_style =
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

    static unordered_map<VertexType, string> const vt2s = {
        {VertexType::boundary, "boun"},
        {VertexType::z, "zspi"},
        {VertexType::x, "xspi"},
        {VertexType::h_box, "hbox"}};

    static unordered_map<EdgeType, string> const et2s = {
        {EdgeType::hadamard, "hedg"},
        {EdgeType::simple, "sedg"},
    };

    constexpr string_view font_size = "\\tiny";

    size_t max = 0;

    for (auto& v : _outputs) {
        if (max < v->get_col())
            max = v->get_col();
    }
    for (auto& v : _inputs) {
        if (max < v->get_col())
            max = v->get_col();
    }
    double scale = (double)25 / (double)static_cast<int>(max);
    scale = (scale > 3.0) ? 3.0 : scale;
    filename << define_colors;
    filename << "\\scalebox{" << to_string(scale) << "}{";
    filename << "\\begin{tikzpicture}" << tikz_style;
    filename << "    % Vertices\n";

    auto write_phase = [&filename, &font_size](ZXVertex* v) {
        if (v->get_phase() == Phase(0) && v->get_type() != VertexType::h_box)
            return true;
        if (v->get_phase() == Phase(1) && v->get_type() == VertexType::h_box)
            return true;
        string label_style = "[label distance=-2]90:{\\color{phaseColor}";
        filename << ",label={ " << label_style << font_size << " $";
        int numerator = v->get_phase().numerator();
        int denominator = v->get_phase().denominator();

        if (denominator != 1) {
            filename << "\\frac{";
        }
        if (numerator != 1) {
            filename << "\\mathsf{" << to_string(numerator) << "}";
        }
        filename << "\\pi";
        if (denominator != 1) {
            filename << "}{ \\mathsf{" << to_string(denominator) << "}}";
        }
        filename << "$ }}";
        return true;
    };

    // NOTE - Sample: \node[zspi] (88888)  at (0,1) {{\tiny 88888}};
    for (auto& v : _vertices) {
        filename << "    \\node[" << vt2s.at(v->get_type());
        write_phase(v);
        filename << "]";
        filename << "(" << to_string(v->get_id()) << ")  at (" << to_string(v->get_col()) << "," << to_string(v->get_qubit()) << ") ";
        filename << "{{" << font_size << " " << to_string(v->get_id()) << "}};\n";
    }
    // NOTE - Sample: \draw[hedg] (1234) -- (123);
    filename << "    % Edges\n";

    for (auto& v : _vertices) {
        for (auto& [n, e] : v->get_neighbors()) {
            if (n->get_id() > v->get_id()) {
                if (n->get_col() == v->get_col() && n->get_qubit() == v->get_qubit()) {
                    LOGGER.warning("{} and {} are connected but they have same coordinates.", v->get_id(), n->get_id());
                    filename << "    % \\draw[" << et2s.at(e) << "] (" << to_string(v->get_id()) << ") -- (" << to_string(n->get_id()) << ");\n";
                } else
                    filename << "    \\draw[" << et2s.at(e) << "] (" << to_string(v->get_id()) << ") -- (" << to_string(n->get_id()) << ");\n";
            }
        }
    }

    filename << "\\end{tikzpicture}}\n";
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
bool ZXGraph::write_pdf(string const& filename) const {
    namespace fs = std::filesystem;
    namespace dv = dvlab::utils;
    fs::path filepath{filename};

    if (filepath.extension() == "") {
        LOGGER.error("no file extension!!");
        return false;
    }

    if (filepath.extension() != ".pdf") {
        LOGGER.error("unsupported file extension \"{}\"!!", filepath.extension().string());
        return false;
    }

    filepath.replace_extension(".tex");
    if (filepath.parent_path().empty()) {
        filepath = "./" + filepath.string();
    }

    std::error_code ec;
    fs::create_directory(filepath.parent_path(), ec);
    if (ec) {
        LOGGER.error("failed to create the directory");
        LOGGER.error("{}", ec.message());
        return false;
    }

    dv::TmpDir tmp_dir;

    auto temp_tex_path = tmp_dir.path() / filepath.filename();

    fstream tex_file{temp_tex_path, ios::out};
    if (!tex_file.is_open()) {
        LOGGER.error("Cannot open the file \"{}\"!!", filepath.string());
        return false;
    }

    if (!write_tex(tex_file)) return false;

    tex_file.close();
    // NOTE - Linux cmd: pdflatex -halt-on-error -output-directory <path/to/dir> <path/to/tex>
    string cmd = "pdflatex -halt-on-error -output-directory " + temp_tex_path.parent_path().string() + " " + temp_tex_path.string() + " >/dev/null 2>&1 ";
    if (system(cmd.c_str()) == -1) {
        LOGGER.error("failed to generate PDF");
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
bool ZXGraph::write_tex(string const& filename) const {
    namespace fs = std::filesystem;
    fs::path filepath{filename};

    if (filepath.extension() == "") {
        LOGGER.error("no file extension!!");
        return false;
    }

    if (filepath.extension() != ".tex") {
        LOGGER.error("unsupported file extension \"{}\"!!", filepath.extension().string());
        return false;
    }

    if (!filepath.parent_path().empty()) {
        std::error_code ec;
        fs::create_directory(filepath.parent_path(), ec);
        if (ec) {
            LOGGER.error("failed to create the directory");
            LOGGER.error("{}", ec.message());
            return false;
        }
    }

    fstream tex_file{filepath, ios::out};
    if (!tex_file.is_open()) {
        LOGGER.error("Cannot open the file \"{}\"!!", filepath.string());
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
bool ZXGraph::write_tex(ostream& filename) const {
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
    filename << includes;
    filename << "\\begin{document}\n";
    if (!write_tikz(filename)) {
        LOGGER.error("Failed to write tikz");
        return false;
    }
    filename << "\\end{document}\n";
    return true;
}
