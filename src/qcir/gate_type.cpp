/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define qcir gate types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir/gate_type.hpp"

#include "qcir/qcir_translate.hpp"
#include "util/util.hpp"

namespace qsyn::qcir {

std::optional<Operation> str_to_operation(std::string const& str, std::vector<dvlab::Phase> const& params) {
    if (params.empty()) {
        if (str == "id") return IdGate();
        if (str == "h") return HGate();
        if (str == "swap") return SwapGate();
        if (str == "ecr") return ECRGate();
        if (str == "z") return ZGate();
        if (str == "s") return SGate();
        if (str == "sdg") return SdgGate();
        if (str == "t") return TGate();
        if (str == "tdg") return TdgGate();
    }
    if (params.size() == 1) {
        if (str == "p" || str == "pz") return PZGate(params[0]);
    }

    return std::nullopt;
}

Operation adjoint(LegacyGateType const& op) {
    return LegacyGateType(std::make_tuple(op.get_rotation_category(), op.get_num_qubits(), -op.get_phase()));
}

bool is_clifford(LegacyGateType const& op) {
    return (op.get_num_qubits() == 1 && op.get_phase().denominator() <= 2) || (op.get_num_qubits() == 2 && op.get_phase().denominator() == 1);
}

std::optional<GateType> str_to_gate_type(std::string_view str) {
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
    if (str == "sdg")
        return GateType{GateRotationCategory::pz, num_qubits, dvlab::Phase(-1, 2)};
    if (str == "t")
        return GateType{GateRotationCategory::pz, num_qubits, dvlab::Phase(1, 4)};
    if (str == "tdg")
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

    if (str == "sxdg")
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
    if (str == "sydg")
        return GateType{GateRotationCategory::py, num_qubits, dvlab::Phase(-1, 2)};

    return std::nullopt;
}
std::string gate_type_to_str(GateRotationCategory category, std::optional<size_t> num_qubits, std::optional<dvlab::Phase> phase) {
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
            default:
                DVLAB_UNREACHABLE("Should be unreachable!!");
        }
    });

    return type_str;
}

std::string gate_type_to_str(GateType const& type) {
    return gate_type_to_str(std::get<0>(type), std::get<1>(type), std::get<2>(type));
}

dvlab::utils::ordered_hashmap<std::string, Equivalence> EQUIVALENCE_LIBRARY = {
    {"sherbrooke", {{"h", {
                              {"s", {0}, dvlab::Phase(0)},
                              {"sx", {0}, dvlab::Phase(0)},
                              {"s", {0}, dvlab::Phase(0)},
                          }},
                    {"cx", {
                               {"sdg", {0}, dvlab::Phase(0)},
                               {"z", {1}, dvlab::Phase(0)},
                               {"sx", {1}, dvlab::Phase(0)},
                               {"z", {1}, dvlab::Phase(0)},
                               {"ecr", {0, 1}, dvlab::Phase(0)},
                               {"x", {0}, dvlab::Phase(0)},
                           }},
                    {"cz", {
                               {"sdg", {0}, dvlab::Phase(0)},
                               {"sx", {1}, dvlab::Phase(0)},
                               {"s", {1}, dvlab::Phase(0)},
                               {"ecr", {0, 1}, dvlab::Phase(0)},
                               {"x", {0}, dvlab::Phase(0)},
                               {"s", {1}, dvlab::Phase(0)},
                               {"sx", {1}, dvlab::Phase(0)},
                               {"s", {1}, dvlab::Phase(0)},
                           }}}},
    {"kyiv", {{"h", {
                        {"s", {0}, dvlab::Phase(0)},
                        {"sx", {0}, dvlab::Phase(0)},
                        {"s", {0}, dvlab::Phase(0)},
                    }},
              {"cz", {
                         {"s", {1}, dvlab::Phase(0)},
                         {"sx", {1}, dvlab::Phase(0)},
                         {"s", {1}, dvlab::Phase(0)},
                         {"cx", {0, 1}, dvlab::Phase(0)},
                         {"s", {1}, dvlab::Phase(0)},
                         {"sx", {1}, dvlab::Phase(0)},
                         {"s", {1}, dvlab::Phase(0)},
                     }}}},
    {"prague", {{"h", {
                          {"s", {0}, dvlab::Phase(0)},
                          {"sx", {0}, dvlab::Phase(0)},
                          {"s", {0}, dvlab::Phase(0)},
                      }},
                {"cx", {
                           {"s", {1}, dvlab::Phase(0)},
                           {"sx", {1}, dvlab::Phase(0)},
                           {"z", {1}, dvlab::Phase(0)},
                           {"cz", {0, 1}, dvlab::Phase(0)},
                           {"sx", {1}, dvlab::Phase(0)},
                           {"s", {1}, dvlab::Phase(0)},
                       }}}},
};

}  // namespace qsyn::qcir
