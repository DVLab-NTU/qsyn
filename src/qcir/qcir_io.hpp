
#pragma once

#include "./qcir.hpp"

namespace qsyn::qcir {
std::optional<QCir> from_file(std::filesystem::path const& filepath);
std::optional<QCir> from_qasm(std::filesystem::path const& filepath);
std::optional<QCir> from_qc(std::filesystem::path const& filepath);

std::string to_qasm(QCir const& qcir);
}  // namespace qsyn::qcir
