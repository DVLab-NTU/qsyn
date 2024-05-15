/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define qcir gate types ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir/basic_gate_type.hpp"
#include "qcir/operation.hpp"
#include "qcir/qcir_translate.hpp"
#include "util/dvlab_string.hpp"

namespace qsyn::qcir {

namespace {
std::optional<Operation> str_to_basic_operation(std::string str, std::vector<dvlab::Phase> const& params) {
    str = dvlab::str::tolower_string(str);
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

        if (str == "x" || str == "not") return XGate();
        if (str == "sx" || str == "x_1_2") return SXGate();
        if (str == "sxdg") return SXdgGate();
        if (str == "tx") return TXGate();
        if (str == "txdg") return TXdgGate();

        if (str == "y") return YGate();
        if (str == "sy" || str == "y_1_2") return SYGate();
        if (str == "sydg") return SYdgGate();
        if (str == "ty") return TYGate();
        if (str == "tydg") return TYdgGate();
    }
    if (params.size() == 1) {
        if (str == "p" || str == "pz") return PZGate(params[0]);
        if (str == "px") return PXGate(params[0]);
        if (str == "py") return PYGate(params[0]);
        if (str == "rz") return RZGate(params[0]);
        if (str == "rx") return RXGate(params[0]);
        if (str == "ry") return RYGate(params[0]);
    }

    return std::nullopt;
}
}  // namespace

std::optional<Operation> str_to_operation(std::string str, std::vector<dvlab::Phase> const& params) {
    str = dvlab::str::tolower_string(str);

    auto const n_ctrls = str.find_first_not_of('c');
    str                = str.substr(n_ctrls);

    auto basic_op_type = str_to_basic_operation(str, params);

    if (!basic_op_type.has_value()) return std::nullopt;

    if (n_ctrls > 0) {
        return ControlGate(*basic_op_type, n_ctrls);
    }

    return basic_op_type;
}

}  // namespace qsyn::qcir
