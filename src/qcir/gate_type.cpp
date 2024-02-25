/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define qcir gate types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir/gate_type.hpp"

#include "util/util.hpp"

namespace qsyn::qcir {

std::optional<GateType> str_to_gate_type(std::string_view str) {
    // Misc
    if (str == "id")
        return GateType{GateRotationCategory::id, 1, dvlab::Phase(0)};
    if (str == "h")
        return GateType{GateRotationCategory::h, 1, dvlab::Phase(1)};
    if (str == "swap")
        return GateType{GateRotationCategory::swap, 2, dvlab::Phase(1)};
    if (str == "ecr")
        return GateType{GateRotationCategory::ecr, 2, dvlab::Phase(0)};

    std::optional<size_t> num_qubits = 1;
    if (str.starts_with("mc")) {
        num_qubits = std::nullopt;
        str.remove_prefix(2);
    } else {
        while (str.starts_with("c")) {
            (*num_qubits)++;
            str.remove_prefix(1);
        }
    }
    // single-qubit Z-rotation gates
    if (str == "pz" || str == "p")
        return GateType{GateRotationCategory::pz, num_qubits, std::nullopt};
    if (str == "rz")
        return GateType{GateRotationCategory::rz, num_qubits, std::nullopt};
    if (str == "z")
        return GateType{GateRotationCategory::pz, num_qubits, dvlab::Phase(1)};
    if (str == "s")
        return GateType{GateRotationCategory::pz, num_qubits, dvlab::Phase(1, 2)};
    if (str == "s*" || str == "sdg" || str == "sd")
        return GateType{GateRotationCategory::pz, num_qubits, dvlab::Phase(-1, 2)};
    if (str == "t")
        return GateType{GateRotationCategory::pz, num_qubits, dvlab::Phase(1, 4)};
    if (str == "t*" || str == "tdg" || str == "td")
        return GateType{GateRotationCategory::pz, num_qubits, dvlab::Phase(-1, 4)};

    // single-qubit X-rotation gates
    if (str == "px")
        return GateType{GateRotationCategory::px, num_qubits, std::nullopt};

    if (str == "rx")
        return GateType{GateRotationCategory::rx, num_qubits, std::nullopt};

    if (str == "x" || str == "not")
        return GateType{GateRotationCategory::px, num_qubits, dvlab::Phase(1)};

    if (str == "sx" || str == "x_1_2")
        return GateType{GateRotationCategory::px, num_qubits, dvlab::Phase(1, 2)};

    if (str == "sx*" || str == "sxdg" || str == "sxd")
        return GateType{GateRotationCategory::px, num_qubits, dvlab::Phase(-1, 2)};

    // single-qubit Y-rotation gates
    if (str == "py")
        return GateType{GateRotationCategory::py, num_qubits, std::nullopt};
    if (str == "ry")
        return GateType{GateRotationCategory::ry, num_qubits, std::nullopt};
    if (str == "y")
        return GateType{GateRotationCategory::py, num_qubits, dvlab::Phase(1)};
    if (str == "sy" || str == "y_1_2")
        return GateType{GateRotationCategory::py, num_qubits, dvlab::Phase(1, 2)};
    if (str == "sy*" || str == "sydg" || str == "syd")
        return GateType{GateRotationCategory::py, num_qubits, dvlab::Phase(-1, 2)};

    if (str == "ecr")
        return GateType{GateRotationCategory::ecr, 2, dvlab::Phase(0)};

    return std::nullopt;
}
std::string gate_type_to_str(GateRotationCategory category, std::optional<size_t> num_qubits, std::optional<dvlab::Phase> phase) {
    DVLAB_ASSERT(num_qubits > 0, "a gate should have at least one qubit");
    if (category == GateRotationCategory::id)
        return "id";
    if (category == GateRotationCategory::h)
        return "h";
    if (category == GateRotationCategory::swap)
        return "swap";
    if (category == GateRotationCategory::ecr)
        return "ecr";

    std::string type_str = std::invoke([num_qubits]() {
        if (!num_qubits.has_value()) {
            return std::string{"mc"};
        } else
            return std::string(num_qubits.value() - 1, 'c');
    });

    type_str += std::invoke([phase, category]() {
        switch (category) {
            using dvlab::Phase;
            case GateRotationCategory::pz: {
                if (phase == Phase(0)) {
                    return "id";
                }
                if (phase == Phase(1)) {
                    return "z";
                }
                if (phase == Phase(1, 2)) {
                    return "s";
                }
                if (phase == Phase(-1, 2)) {
                    return "sdg";
                }
                if (phase == Phase(1, 4)) {
                    return "t";
                }
                if (phase == Phase(-1, 4)) {
                    return "tdg";
                }
                return "p";
            }
            case GateRotationCategory::px: {
                if (phase == Phase(0)) {
                    return "id";
                }
                if (phase == Phase(1)) {
                    return "x";
                }
                if (phase == Phase(1, 2)) {
                    return "sx";
                }
                if (phase == Phase(-1, 2)) {
                    return "sxdg";
                }
                if (phase == Phase(1, 4)) {
                    return "tx";
                }
                if (phase == Phase(-1, 4)) {
                    return "txdg";
                }
                return "px";
            }
            case GateRotationCategory::py: {
                if (phase == Phase(0)) {
                    return "id";
                }
                if (phase == Phase(1)) {
                    return "y";
                }
                if (phase == Phase(1, 2)) {
                    return "sy";
                }
                if (phase == Phase(-1, 2)) {
                    return "sydg";
                }
                if (phase == Phase(1, 4)) {
                    return "ty";
                }
                if (phase == Phase(-1, 4)) {
                    return "tydg";
                }
                return "py";
            }
            case GateRotationCategory::rz:
                return "rz";
            case GateRotationCategory::rx:
                return "rx";
            case GateRotationCategory::ry:
                return "ry";
            case GateRotationCategory::id:
            case GateRotationCategory::h:
            case GateRotationCategory::swap:
            default:
                DVLAB_UNREACHABLE("Should be unreachable!!");
        }
    });

    return type_str;
}

std::string gate_type_to_str(GateType const& type) {
    return gate_type_to_str(std::get<0>(type), std::get<1>(type), std::get<2>(type));
}

bool is_fixed_phase_gate(GateRotationCategory category) {
    return category == GateRotationCategory::id ||
           category == GateRotationCategory::h ||
           category == GateRotationCategory::swap;
}

dvlab::Phase get_fixed_phase(GateRotationCategory category) {
    assert(is_fixed_phase_gate(category));

    switch (category) {
        case GateRotationCategory::id:
            return dvlab::Phase(0);
        case GateRotationCategory::h:
        case GateRotationCategory::swap:
            return dvlab::Phase(1);
        default:
            assert(false);
    }
}

}  // namespace qsyn::qcir
