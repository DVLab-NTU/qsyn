/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define conversion among QCir, ZXGraph, and Tensor ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./conversion_cmd.hpp"

#include <spdlog/spdlog.h>

#include <string>

#include "./qcir_to_tableau.hpp"
#include "./qcir_to_tensor.hpp"
#include "./qcir_to_zxgraph.hpp"
#include "./tableau_to_qcir.hpp"
#include "./zxgraph_to_tensor.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "convert/qcir_to_tableau.hpp"
#include "extractor/extract.hpp"
#include "qcir/qcir_mgr.hpp"
#include "tensor/decomposer.hpp"
#include "tensor/tensor_mgr.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/dvlab_string.hpp"
#include "util/util.hpp"

using namespace dvlab::argparse;

using dvlab::Command, dvlab::CmdExecResult;

namespace qsyn {

bool valid_decomposition_mode(size_t const& val) {
    if (val < 4) return true;
    spdlog::error("Decomposition Mode {} is not valid!!", val);
    return false;
};

Command conversion_cmd(QCirMgr& qcir_mgr, qsyn::tensor::TensorMgr& tensor_mgr, qsyn::zx::ZXGraphMgr& zxgraph_mgr, experimental::TableauMgr& tableau_mgr) {
    return {"convert",
            [&](ArgumentParser& parser) {
                parser.description("conversion among QCir, ZXGraph, and Tensor");

                parser.add_argument<std::string>("from")
                    .constraint(choices_allow_prefix({"qcir", "zx", "tensor", "tableau"}))
                    // The tensor data structure currently is not supported as the source data structure.
                    // We still allow the user to specify it, but an error will be thrown.
                    .help("specify the source data structure. Choices: qcir, zx, tableau");

                parser.add_argument<std::string>("to")
                    .constraint(choices_allow_prefix({"qcir", "zx", "tensor", "tableau"}))
                    .help("specify the destination data structure. Choices: qcir, zx, tensor, tableau");

                parser.add_argument<size_t>("decomp-mode")
                    .default_value(3)
                    .constraint(valid_decomposition_mode)
                    .help("specify the decomposition mode (default: 3). The higher the number, the more aggressive the decomposition is. This option is currently only meaningful when converting from QCir to ZXGraph.");
            },
            [&](ArgumentParser const& parser) {
                using namespace std::string_view_literals;
                auto const from = parser.get<std::string>("from");
                auto const to   = parser.get<std::string>("to");

                if (from == to) {
                    spdlog::error("The source and destination data structure should not be the same!!", from, to);
                    return CmdExecResult::error;
                }
                enum class data_type {
                    qcir,
                    zx,
                    tensor,
                    tableau
                };
                auto constexpr get_data_type = [](std::string_view pfx) -> data_type {
                    if (dvlab::str::is_prefix_of(dvlab::str::tolower_string(pfx), "qcir")) return data_type::qcir;
                    if (dvlab::str::is_prefix_of(dvlab::str::tolower_string(pfx), "zx")) return data_type::zx;
                    if (dvlab::str::is_prefix_of(dvlab::str::tolower_string(pfx), "tensor")) return data_type::tensor;
                    if (dvlab::str::is_prefix_of(dvlab::str::tolower_string(pfx), "tableau")) return data_type::tableau;
                    DVLAB_UNREACHABLE("Invalid data type");
                };

                auto const is_conversion_type = [from_arg = get_data_type(from), to_arg = get_data_type(to)](data_type from, data_type to) {
                    return from == from_arg && to == to_arg;
                };

                if (is_conversion_type(data_type::qcir, data_type::zx)) {
                    if (!dvlab::utils::mgr_has_data(qcir_mgr)) return CmdExecResult::error;
                    if (to == "zx") {
                        spdlog::info("Converting to QCir {} to ZXGraph {}...", qcir_mgr.focused_id(), zxgraph_mgr.get_next_id());
                        auto const g = to_zxgraph(*qcir_mgr.get(), parser.get<size_t>("decomp-mode"));

                        if (g.has_value()) {
                            zxgraph_mgr.add(zxgraph_mgr.get_next_id(), std::make_unique<qsyn::zx::ZXGraph>(std::move(g.value())));

                            zxgraph_mgr.get()->set_filename(qcir_mgr.get()->get_filename());
                            zxgraph_mgr.get()->add_procedures(qcir_mgr.get()->get_procedures());
                            zxgraph_mgr.get()->add_procedure("QC2ZX");
                        }

                        return CmdExecResult::done;
                    }
                }
                if (is_conversion_type(data_type::qcir, data_type::tensor)) {
                    if (!dvlab::utils::mgr_has_data(qcir_mgr)) return CmdExecResult::error;
                    spdlog::info("Converting to QCir {} to Tensor {}...", qcir_mgr.focused_id(), tensor_mgr.get_next_id());
                    auto tensor = to_tensor(*qcir_mgr.get());

                    if (tensor.has_value()) {
                        tensor_mgr.add(tensor_mgr.get_next_id());
                        tensor_mgr.set(std::make_unique<qsyn::tensor::QTensor<double>>(std::move(tensor.value())));

                        tensor_mgr.get()->set_filename(qcir_mgr.get()->get_filename());
                        tensor_mgr.get()->add_procedures(qcir_mgr.get()->get_procedures());
                        tensor_mgr.get()->add_procedure("QC2TS");
                    }

                    return CmdExecResult::done;
                }

                if (is_conversion_type(data_type::zx, data_type::tensor)) {
                    if (!dvlab::utils::mgr_has_data(zxgraph_mgr)) return CmdExecResult::error;
                    auto zx = zxgraph_mgr.get();

                    spdlog::info("Converting ZXGraph {} to Tensor {}...", zxgraph_mgr.focused_id(), tensor_mgr.get_next_id());
                    auto tensor = qsyn::to_tensor(*zx);

                    if (tensor.has_value()) {
                        tensor_mgr.add(tensor_mgr.get_next_id(), std::make_unique<qsyn::tensor::QTensor<double>>(std::move(tensor.value())));

                        tensor_mgr.get()->set_filename(zx->get_filename());
                        tensor_mgr.get()->add_procedures(zx->get_procedures());
                        tensor_mgr.get()->add_procedure("ZX2TS");
                    }

                    return CmdExecResult::done;
                }

                if (is_conversion_type(data_type::zx, data_type::qcir)) {
                    if (!dvlab::utils::mgr_has_data(zxgraph_mgr)) return CmdExecResult::error;
                    if (!zxgraph_mgr.get()->is_graph_like()) {
                        spdlog::error("ZXGraph {} is not extractable because it is not graph-like!!", zxgraph_mgr.focused_id());
                        return CmdExecResult::error;
                    }
                    auto next_id = zxgraph_mgr.get_next_id();
                    zxgraph_mgr.copy(next_id);
                    extractor::Extractor ext(zxgraph_mgr.get(), nullptr, std::nullopt);

                    qcir::QCir* result = ext.extract();
                    if (result != nullptr) {
                        qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<qcir::QCir>(*result));
                        if (extractor::PERMUTE_QUBITS)
                            zxgraph_mgr.remove(next_id);
                        else {
                            spdlog::warn("The extracted circuit is up to a qubit permutation.");
                            spdlog::warn("Remaining permutation information is in ZXGraph id {}.", next_id);
                            zxgraph_mgr.get()->add_procedure("ZX2QC");
                        }

                        qcir_mgr.get()->add_procedures(zxgraph_mgr.get()->get_procedures());
                        qcir_mgr.get()->add_procedure("ZX2QC");
                        qcir_mgr.get()->set_filename(zxgraph_mgr.get()->get_filename());
                    }

                    return CmdExecResult::done;
                }

                // ts2qc

                if (is_conversion_type(data_type::tensor, data_type::qcir)) {
                    if (!dvlab::utils::mgr_has_data(tensor_mgr)) return CmdExecResult::error;

                    spdlog::info("Converting Tensor {} to QCir {}...", tensor_mgr.focused_id(), qcir_mgr.get_next_id());
                    auto const result = tensor::Decomposer{}.decompose(*tensor_mgr.get());

                    if (result) {
                        qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<qcir::QCir>(std::move(*result)));
                        qcir_mgr.get()->add_procedures(tensor_mgr.get()->get_procedures());
                        qcir_mgr.get()->add_procedure("TS2QC");
                        qcir_mgr.get()->set_filename(tensor_mgr.get()->get_filename());
                    }
                    return CmdExecResult::done;
                }
                if (is_conversion_type(data_type::qcir, data_type::tableau)) {
                    if (!dvlab::utils::mgr_has_data(qcir_mgr)) return CmdExecResult::error;
                    spdlog::info("Converting to QCir {} to Tableau {}...", qcir_mgr.focused_id(), tableau_mgr.get_next_id());
                    auto tableau = experimental::to_tableau(*qcir_mgr.get());

                    if (!tableau.has_value()) {
                        spdlog::error("Failed to convert QCir {} to Tableau!!", qcir_mgr.focused_id());
                        return CmdExecResult::error;
                    }

                    tableau_mgr.add(tableau_mgr.get_next_id(), std::make_unique<experimental::Tableau>(std::move(tableau.value())));

                    tableau_mgr.get()->set_filename(qcir_mgr.get()->get_filename());
                    tableau_mgr.get()->add_procedures(qcir_mgr.get()->get_procedures());
                    tableau_mgr.get()->add_procedure("QC2TB");

                    return CmdExecResult::done;
                }

                if (is_conversion_type(data_type::tableau, data_type::qcir)) {
                    if (!dvlab::utils::mgr_has_data(tableau_mgr)) return CmdExecResult::error;
                    spdlog::info("Converting to Tableau {} to QCir {}...", tableau_mgr.focused_id(), qcir_mgr.get_next_id());
                    auto qcir = experimental::to_qcir(*tableau_mgr.get(), experimental::HOptSynthesisStrategy{});

                    qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<qcir::QCir>(std::move(qcir)));

                    qcir_mgr.get()->set_filename(tableau_mgr.get()->get_filename());
                    qcir_mgr.get()->add_procedures(tableau_mgr.get()->get_procedures());
                    qcir_mgr.get()->add_procedure("TB2QC");

                    return CmdExecResult::done;
                }

                spdlog::error("Conversion from {} to {} is not yet supported!!", from, to);

                return CmdExecResult::error;
            }};
}

bool add_conversion_cmds(dvlab::CommandLineInterface& cli, QCirMgr& qcir_mgr, qsyn::tensor::TensorMgr& tensor_mgr, qsyn::zx::ZXGraphMgr& zxgraph_mgr, experimental::TableauMgr& tableau_mgr) {
    if (!(cli.add_command(conversion_cmd(qcir_mgr, tensor_mgr, zxgraph_mgr, tableau_mgr)) &&
          cli.add_alias("qc2zx", "convert qcir zx") &&
          cli.add_alias("qc2ts", "convert qcir tensor") &&
          cli.add_alias("zx2ts", "convert zx tensor") &&
          cli.add_alias("zx2qc", "convert zx qcir") &&
          cli.add_alias("ts2qc", "convert tensor qcir") &&
          cli.add_alias("qc2tb", "convert qcir tableau") &&
          cli.add_alias("tb2qc", "convert tableau qcir"))) {
        fmt::println(stderr, "Registering \"conversion\" commands fails... exiting");
        return false;
    }
    return true;
}

}  // namespace qsyn
