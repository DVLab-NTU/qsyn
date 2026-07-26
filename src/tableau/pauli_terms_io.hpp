/**
 * @file pauli_terms_io.hpp
 * @brief Load Pauli Hamiltonian / rotation terms from JSON or .terms files (PySCF export).
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "util/phase.hpp"

namespace qsyn::experimental {

struct PauliTermsFileMeta {
    int version         = 1;
    std::string molecule;
    std::string basis;
    std::string encoding;
    size_t n_qubits     = 0;
};

struct PauliTermEntry {
    size_t id           = 0;
    std::string pauli;
    std::optional<double> coeff;
    std::optional<dvlab::Phase> angle;
};

struct PauliTermsLoadOptions {
    bool use_coeff      = false;
    bool use_angle      = true;
    double scale        = 1.0;
};

struct PauliTermsFile {
    PauliTermsFileMeta meta;
    std::vector<PauliTermEntry> terms;
};

[[nodiscard]] std::optional<PauliTermsFile> load_pauli_terms_file(std::filesystem::path const& path);

[[nodiscard]] std::optional<std::vector<std::pair<std::string, dvlab::Phase>>> to_pauli_phase_pairs(
    PauliTermsFile const& file,
    PauliTermsLoadOptions const& options);

void print_pauli_terms_loaded(PauliTermsFile const& file, size_t tableau_id);

}  // namespace qsyn::experimental
