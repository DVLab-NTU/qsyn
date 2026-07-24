/****************************************************************************
  PackageName  [ tableau / cpf ]
  Synopsis     [ Optional JSONL merge logger (env CPF_MERGE_LOG=<path>). ]
****************************************************************************/

#pragma once

#include <fmt/core.h>

#include <cstdlib>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>

#include "tableau/pauli_rotation.hpp"

namespace qsyn::experimental::cpf {

inline std::ofstream* merge_log_stream() {
    static std::once_flag once;
    static std::ofstream* stream = nullptr;
    std::call_once(once, [] {
        if (char const* path = std::getenv("CPF_MERGE_LOG"); path != nullptr && path[0] != '\0') {
            stream = new std::ofstream(path);
        }
    });
    return stream;
}

inline bool merge_log_enabled() { return merge_log_stream() != nullptr; }

inline std::string format_clifford_ops(CliffordOperatorString const& ops) {
    std::string out;
    for (auto const& [type, qs] : ops) {
        if (!out.empty()) out += ';';
        auto const name = to_string(type);
        if (type == CliffordOperatorType::cx || type == CliffordOperatorType::cz ||
            type == CliffordOperatorType::swap || type == CliffordOperatorType::ecr) {
            out += fmt::format("{}({},{})", name, qs[0], qs[1]);
        } else {
            out += fmt::format("{}({})", name, qs[0]);
        }
    }
    return out;
}

inline void merge_log_line(std::string const& line) {
    if (auto* os = merge_log_stream()) {
        (*os) << line << '\n';
        os->flush();
    }
}

}  // namespace qsyn::experimental::cpf
