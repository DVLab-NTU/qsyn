#include "./classical_tableau.hpp"
#include "./tableau.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace qsyn::experimental {

namespace {

struct ParsedCommuteTestCase {
    size_t n_qubits = 0;
    size_t pmc_reference = 0;
    size_t pmc_ancilla = 0;
    CliffordOperatorString expected_ops;
    std::vector<SubTableau> commute_members;
};

enum class ParseSection : unsigned char {
    none,
    ops,
    commute
};

std::string trim_copy(std::string_view sv) {
    auto const is_not_space = [](unsigned char ch) {
        return !std::isspace(ch);
    };
    while (!sv.empty() && !is_not_space(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && !is_not_space(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return std::string{sv};
}

std::string to_lower_copy(std::string s) {
    std::ranges::transform(s, s.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return s;
}

std::string normalize_phase_token(std::string token) {
    static std::string const pi_utf8 = "\xCF\x80";
    size_t pos = 0;
    while ((pos = token.find(pi_utf8, pos)) != std::string::npos) {
        token.replace(pos, pi_utf8.size(), "pi");
        pos += 2;
    }
    return token;
}

std::optional<CliffordOperator> parse_gate_line(std::string const& line) {
    static std::regex const two_qubit_gate_re{
        R"(^([A-Za-z][A-Za-z0-9]*)\s+q\[(\d+)\]\s*,\s*q\[(\d+)\]\s*;\s*$)"};
    static std::regex const one_qubit_gate_re{
        R"(^([A-Za-z][A-Za-z0-9]*)\s+q\[(\d+)\]\s*;\s*$)"};
    std::smatch match;

    if (std::regex_match(line, match, two_qubit_gate_re)) {
        auto const gate_type = to_clifford_operator_type(to_lower_copy(match[1].str()));
        if (!gate_type.has_value()) {
            throw std::runtime_error(fmt::format("Unknown two-qubit gate '{}'", match[1].str()));
        }
        return CliffordOperator{
            *gate_type,
            {static_cast<size_t>(std::stoull(match[2].str())), static_cast<size_t>(std::stoull(match[3].str()))}};
    }
    if (std::regex_match(line, match, one_qubit_gate_re)) {
        auto const gate_type = to_clifford_operator_type(to_lower_copy(match[1].str()));
        if (!gate_type.has_value()) {
            throw std::runtime_error(fmt::format("Unknown one-qubit gate '{}'", match[1].str()));
        }
        return CliffordOperator{
            *gate_type,
            {static_cast<size_t>(std::stoull(match[2].str())), 0}};
    }
    return std::nullopt;
}

std::optional<ClassicalControlTableau> parse_gadget_line(std::string const& line, size_t n_qubits) {
    static std::regex const gadget_re{R"(^gadget\s*\(\s*(\d+)\s*,\s*(\d+)\s*\)\s*$)"};
    std::smatch match;
    if (!std::regex_match(line, match, gadget_re)) {
        return std::nullopt;
    }
    size_t const reference = static_cast<size_t>(std::stoull(match[1].str()));
    size_t const ancilla = static_cast<size_t>(std::stoull(match[2].str()));
    return ClassicalControlTableau{ancilla, reference, n_qubits, CCTType::Gadget};
}

std::optional<PauliRotation> parse_pr_line(std::string const& line, size_t n_qubits) {
    static std::regex const pr_re{R"(^(?:(\d+)\s+)?([01]+)\s+(.+?)\s*$)"};
    std::smatch match;
    if (!std::regex_match(line, match, pr_re)) {
        return std::nullopt;
    }

    auto const bit_string = match[2].str();
    if (bit_string.size() != n_qubits) {
        throw std::runtime_error(
            fmt::format("PR bit-string width mismatch: got {}, expected {}", bit_string.size(), n_qubits));
    }

    auto phase_token = normalize_phase_token(trim_copy(match[3].str()));
    auto const phase = dvlab::Phase::from_string<double>(phase_token);
    if (!phase.has_value()) {
        throw std::runtime_error(fmt::format("Unable to parse phase token '{}'", phase_token));
    }

    std::vector<Pauli> paulis(n_qubits, Pauli::i);
    size_t z_count = 0;
    for (size_t i = 0; i < n_qubits; ++i) {
        if (bit_string[i] == '1') {
            paulis[i] = Pauli::z;
            ++z_count;
        }
    }

    auto rotation = PauliRotation{paulis, *phase};
    if (*phase == dvlab::Phase(0) && z_count == 2) {
        rotation.set_is_CZ(true);
    }
    return rotation;
}

void flush_pending_stabilizer(
    std::vector<CliffordOperator>& pending_st_ops,
    size_t n_qubits,
    std::vector<SubTableau>& commute_members) {
    if (pending_st_ops.empty()) {
        return;
    }
    auto st = StabilizerTableau{n_qubits};
    st.apply(pending_st_ops);
    commute_members.emplace_back(std::move(st));
    pending_st_ops.clear();
}

ParsedCommuteTestCase parse_commute_test_file(std::filesystem::path const& txt_path) {
    std::ifstream in{txt_path};
    if (!in) {
        throw std::runtime_error(fmt::format("Cannot open test file '{}'", txt_path.string()));
    }

    static std::regex const qubits_re{R"(^qubits\s*:\s*(\d+)\s*$)"};
    static std::regex const pmc_re{R"(^PMC\s*\(\s*(\d+)\s*,\s*(\d+)\s*\)\s*$)"};

    ParsedCommuteTestCase parsed;
    bool has_qubits = false;
    bool has_pmc = false;
    bool has_ops = false;
    ParseSection section = ParseSection::none;
    std::vector<CliffordOperator> pending_st_ops;

    std::string raw_line;
    while (std::getline(in, raw_line)) {
        auto line = trim_copy(raw_line);
        if (line.empty()) {
            continue;
        }

        std::smatch match;
        if (std::regex_match(line, match, qubits_re)) {
            parsed.n_qubits = static_cast<size_t>(std::stoull(match[1].str()));
            has_qubits = true;
            continue;
        }
        if (std::regex_match(line, match, pmc_re)) {
            parsed.pmc_reference = static_cast<size_t>(std::stoull(match[1].str()));
            parsed.pmc_ancilla = static_cast<size_t>(std::stoull(match[2].str()));
            has_pmc = true;
            continue;
        }
        if (line == "ops:") {
            section = ParseSection::ops;
            has_ops = true;
            continue;
        }
        if (line == "commute:") {
            section = ParseSection::commute;
            continue;
        }

        if (section == ParseSection::ops) {
            auto op = parse_gate_line(line);
            if (!op.has_value()) {
                throw std::runtime_error(fmt::format("Invalid gate line in ops section: '{}'", line));
            }
            parsed.expected_ops.push_back(*op);
            continue;
        }

        if (section == ParseSection::commute) {
            if (auto const gadget = parse_gadget_line(line, parsed.n_qubits); gadget.has_value()) {
                flush_pending_stabilizer(pending_st_ops, parsed.n_qubits, parsed.commute_members);
                parsed.commute_members.emplace_back(*gadget);
                continue;
            }

            if (auto const op = parse_gate_line(line); op.has_value()) {
                pending_st_ops.push_back(*op);
                continue;
            }

            if (auto const pr = parse_pr_line(line, parsed.n_qubits); pr.has_value()) {
                flush_pending_stabilizer(pending_st_ops, parsed.n_qubits, parsed.commute_members);
                parsed.commute_members.emplace_back(std::vector<PauliRotation>{*pr});
                continue;
            }

            throw std::runtime_error(fmt::format("Invalid commute member line: '{}'", line));
        }

        throw std::runtime_error(fmt::format("Unexpected line before section headers: '{}'", line));
    }

    flush_pending_stabilizer(pending_st_ops, parsed.n_qubits, parsed.commute_members);

    if (!has_qubits) {
        throw std::runtime_error("Missing 'qubits:<N>' header");
    }
    if (!has_pmc) {
        throw std::runtime_error("Missing 'PMC(reference, ancilla)' header");
    }
    if (!has_ops) {
        throw std::runtime_error("Missing 'ops:' section");
    }
    return parsed;
}

}  // namespace

bool run_commute_test_from_file(std::filesystem::path const& txt_path) {
    ParsedCommuteTestCase parsed;
    try {
        parsed = parse_commute_test_file(txt_path);
    } catch (std::exception const& e) {
        spdlog::error("Commute test parse failed: {}", e.what());
        return false;
    }

    auto simulated_pmc = ClassicalControlTableau{
        parsed.pmc_ancilla,
        parsed.pmc_reference,
        parsed.n_qubits,
        CCTType::ClassicalControl};
    simulated_pmc.operations().apply(parsed.expected_ops);

    try {
        for (auto it = parsed.commute_members.rbegin(); it != parsed.commute_members.rend(); ++it) {
            std::visit(
                dvlab::overloaded{
                    [&simulated_pmc](StabilizerTableau& st) {
                        swap(st, simulated_pmc);
                    },
                    [&simulated_pmc](std::vector<PauliRotation>& pr) {
                        swap(pr, simulated_pmc);
                    },
                    [&simulated_pmc](ClassicalControlTableau& cct) {
                        swap(cct, simulated_pmc);
                    }},
                *it);
        }
    } catch (std::exception const& e) {
        spdlog::error("Commute test simulation failed: {}", e.what());
        return false;
    }

    auto const initial_ops_string = clifford_ops_to_string(parsed.expected_ops);
    auto const simulated_ops_string = clifford_ops_to_string(extract_clifford_operators(simulated_pmc.operations()));
    fmt::println("Commute test file: {}", txt_path.string());
    fmt::println("PMC(reference={}, ancilla={})", parsed.pmc_reference, parsed.pmc_ancilla);
    fmt::println("Initial PMC ops (from ops section):");
    fmt::println("{}", initial_ops_string.empty() ? "identity" : initial_ops_string);
    fmt::println("Final PMC ops (after reverse swaps):");
    fmt::println("{}", simulated_ops_string.empty() ? "identity" : simulated_ops_string);
    spdlog::info(
        "Commute test finished for '{}': PMC({}, {})",
        txt_path.string(),
        parsed.pmc_reference,
        parsed.pmc_ancilla);
    spdlog::info("Initial PMC ops (from ops section):\n{}", initial_ops_string.empty() ? "identity\n" : initial_ops_string);
    spdlog::info("Final PMC ops (after reverse swaps):\n{}", simulated_ops_string.empty() ? "identity\n" : simulated_ops_string);
    return true;
}

}  // namespace qsyn::experimental
