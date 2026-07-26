#include "./pauli_terms_io.hpp"

#include <fmt/core.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <cctype>
#include <sstream>

namespace qsyn::experimental {

namespace {

bool is_valid_pauli_string(std::string const& s) {
    return !s.empty() && std::ranges::all_of(s, [](char c) {
        char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return u == 'I' || u == 'X' || u == 'Y' || u == 'Z';
    });
}

std::optional<dvlab::Phase> phase_from_double(double v) {
    std::ostringstream oss;
    oss << v;
    return dvlab::Phase::from_string(oss.str());
}

std::optional<double> parse_key_value_double(std::string const& token, std::string_view key) {
    auto const prefix = std::string{key} + "=";
    if (token.rfind(prefix, 0) != 0) return std::nullopt;
    try {
        return std::stod(token.substr(prefix.size()));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<PauliTermsFile> load_terms_text(std::istream& in) {
    PauliTermsFile file;
    std::string line;
    size_t auto_id = 0;
    while (std::getline(in, line)) {
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        if (line[start] == '#') {
            if (line.find("n_qubits=") != std::string::npos) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    file.meta.n_qubits = static_cast<size_t>(std::stoul(line.substr(pos + 1)));
                }
            }
            continue;
        }
        std::istringstream iss(line.substr(start));
        PauliTermEntry entry;
        if (!(iss >> entry.id)) continue;
        if (!(iss >> entry.pauli)) continue;
        std::string tok;
        while (iss >> tok) {
            if (auto c = parse_key_value_double(tok, "coeff")) entry.coeff = *c;
            if (auto a = parse_key_value_double(tok, "angle")) entry.angle = phase_from_double(*a);
        }
        if (!is_valid_pauli_string(entry.pauli)) {
            spdlog::error("Invalid Pauli string \"{}\" in terms file.", entry.pauli);
            return std::nullopt;
        }
        file.terms.push_back(entry);
        auto_id = std::max(auto_id, entry.id + 1);
    }
    if (file.terms.empty()) {
        spdlog::error("No terms found in text terms file.");
        return std::nullopt;
    }
    if (file.meta.n_qubits == 0) file.meta.n_qubits = file.terms.front().pauli.size();
    (void)auto_id;
    return file;
}

std::optional<PauliTermsFile> load_terms_json(nlohmann::json const& j) {
    PauliTermsFile file;
    file.meta.version   = j.value("version", 1);
    file.meta.molecule  = j.value("molecule", std::string{});
    file.meta.basis     = j.value("basis", std::string{});
    file.meta.encoding  = j.value("encoding", std::string{});
    file.meta.n_qubits  = j.value("n_qubits", size_t{0});

    if (!j.contains("terms") || !j["terms"].is_array()) {
        spdlog::error("JSON terms file missing \"terms\" array.");
        return std::nullopt;
    }
    for (auto const& t : j["terms"]) {
        PauliTermEntry entry;
        entry.id    = t.value("id", file.terms.size());
        entry.pauli = t.value("pauli", std::string{});
        if (t.contains("coeff") && t["coeff"].is_number()) entry.coeff = t["coeff"].get<double>();
        if (t.contains("angle")) {
            if (t["angle"].is_number()) {
                entry.angle = phase_from_double(t["angle"].get<double>());
            } else if (t["angle"].is_string()) {
                entry.angle = dvlab::Phase::from_string(t["angle"].get<std::string>());
            }
        }
        if (!is_valid_pauli_string(entry.pauli)) {
            spdlog::error("Invalid Pauli string \"{}\" in JSON.", entry.pauli);
            return std::nullopt;
        }
        file.terms.push_back(std::move(entry));
    }
    if (file.terms.empty()) {
        spdlog::error("JSON terms file has empty terms array.");
        return std::nullopt;
    }
    if (file.meta.n_qubits == 0) file.meta.n_qubits = file.terms.front().pauli.size();
    return file;
}

}  // namespace

std::optional<PauliTermsFile> load_pauli_terms_file(std::filesystem::path const& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        spdlog::error("Cannot open terms file: {}", path.string());
        return std::nullopt;
    }
    auto const ext = path.extension().string();
    if (ext == ".json") {
        try {
            nlohmann::json j;
            ifs >> j;
            return load_terms_json(j);
        } catch (std::exception const& e) {
            spdlog::error("JSON parse error: {}", e.what());
            return std::nullopt;
        }
    }
    return load_terms_text(ifs);
}

std::optional<std::vector<std::pair<std::string, dvlab::Phase>>> to_pauli_phase_pairs(
    PauliTermsFile const& file,
    PauliTermsLoadOptions const& options) {
    std::vector<std::pair<std::string, dvlab::Phase>> out;
    out.reserve(file.terms.size());
    for (auto const& t : file.terms) {
        std::optional<dvlab::Phase> phase;
        if (options.use_coeff && t.coeff.has_value()) {
            phase = phase_from_double(*t.coeff * options.scale);
        } else if (t.angle.has_value()) {
            if (options.scale != 1.0 && t.coeff.has_value()) {
                phase = phase_from_double(*t.coeff * options.scale);
            } else {
                phase = t.angle;
            }
        } else if (t.coeff.has_value()) {
            phase = phase_from_double(*t.coeff * options.scale);
        }
        if (!phase) {
            spdlog::error("Term #{} ({}) has no angle/coeff.", t.id, t.pauli);
            return std::nullopt;
        }
        out.emplace_back(t.pauli, *phase);
    }
    auto const n = file.meta.n_qubits != 0 ? file.meta.n_qubits : file.terms.front().pauli.size();
    if (!std::ranges::all_of(out, [n](auto const& p) { return p.first.size() == n; })) {
        spdlog::error("All Pauli strings must have length {}.", n);
        return std::nullopt;
    }
    return out;
}

void print_pauli_terms_loaded(PauliTermsFile const& file, size_t tableau_id) {
    std::string meta_desc = file.meta.encoding;
    if (!file.meta.basis.empty()) {
        if (!meta_desc.empty()) meta_desc += ", ";
        meta_desc += file.meta.basis;
    }
    if (!file.meta.molecule.empty()) {
        if (!meta_desc.empty()) meta_desc += ", ";
        meta_desc += file.meta.molecule;
    }
    if (meta_desc.empty()) meta_desc = "terms";
    fmt::println("Loaded {} Pauli terms ({}) → Tableau {}", file.terms.size(), meta_desc, tableau_id);
    for (auto const& t : file.terms) {
        std::string angle_str = t.angle ? t.angle->get_print_string() : "?";
        if (t.coeff) {
            fmt::println("  #{} {}  angle={}  coeff={:.9g}", t.id, t.pauli, angle_str, *t.coeff);
        } else {
            fmt::println("  #{} {}  angle={}", t.id, t.pauli, angle_str);
        }
    }
    fmt::println("Try: tableau optimize collapse && tableau optimize ncf");
}

}  // namespace qsyn::experimental
