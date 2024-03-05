/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define qcir gate types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <cstdint>
#include <functional>
#include <gsl/narrow>
#include <iosfwd>
#include <optional>
#include <type_traits>
#include <utility>

#include "util/phase.hpp"

namespace qsyn::qcir {

enum class GateRotationCategory {
    id,
    h,
    swap,
    pz,
    rz,
    px,
    rx,
    py,
    ry,
    ecr
};

using GateType = std::tuple<GateRotationCategory, std::optional<size_t>, std::optional<dvlab::Phase>>;

std::optional<GateType> str_to_gate_type(std::string_view str);
std::string gate_type_to_str(GateRotationCategory category, std::optional<size_t> num_qubits = std::nullopt, std::optional<dvlab::Phase> phase = std::nullopt);
std::string gate_type_to_str(GateType const& type);

bool is_fixed_phase_gate(GateRotationCategory category);

dvlab::Phase get_fixed_phase(GateRotationCategory category);

}  // namespace qsyn::qcir

template <>
struct fmt::formatter<qsyn::qcir::GateRotationCategory> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(qsyn::qcir::GateRotationCategory const& type, FormatContext& ctx) {
        return fmt::format_to(ctx.out(), "{}", qsyn::qcir::gate_type_to_str(type));
    }
};
