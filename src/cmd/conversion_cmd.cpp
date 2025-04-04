/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define conversion among QCir, ZXGraph, and Tensor ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./conversion_cmd.hpp"

#include <spdlog/spdlog.h>

#include <string>

#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"
#include "cmd/tableau_mgr.hpp"
#include "cmd/tensor_mgr.hpp"
#include "cmd/zxgraph_mgr.hpp"
#include "convert/qcir_to_tableau.hpp"
#include "convert/qcir_to_tensor.hpp"
#include "convert/qcir_to_zxgraph.hpp"
#include "convert/tableau_to_qcir.hpp"
#include "convert/zxgraph_to_tensor.hpp"
#include "extractor/extract.hpp"
#include "qcir/qcir.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tensor/decomposer.hpp"
#include "tensor/solovay_kitaev.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/dvlab_string.hpp"
#include "util/util.hpp"

using namespace dvlab::argparse;

using dvlab::Command, dvlab::CmdExecResult;

namespace qsyn {

namespace extractor {
extern ExtractorConfig EXTRACTOR_CONFIG;
}

Command convert_from_qcir_cmd(
    qcir::QCirMgr& qcir_mgr,
    zx::ZXGraphMgr& zxgraph_mgr,
    tensor::TensorMgr& tensor_mgr,
    experimental::TableauMgr& tableau_mgr) {
    return {
        "qcir",
        [&](ArgumentParser& parser) {
            parser.description("convert from QCir to other data structures");

            auto subparsers = parser.add_subparsers("to-type")
                                  .required(true);

            auto to_zxgraph =
                subparsers.add_parser("zx")
                    .description("convert from QCir to ZXGraph");
            auto to_tensor =
                subparsers.add_parser("tensor")
                    .description("convert from QCir to Tensor");
            auto to_tableau =
                subparsers.add_parser("tableau")
                    .description("convert from QCir to Tableau");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(qcir_mgr)) return CmdExecResult::error;
            auto to_type = parser.get<std::string>("to-type");
            if (to_type == "zx") {
                spdlog::info("Converting to QCir {} to ZXGraph {}...", qcir_mgr.focused_id(), zxgraph_mgr.get_next_id());
                auto graph = to_zxgraph(*qcir_mgr.get());

                if (graph.has_value()) {
                    zxgraph_mgr.add(zxgraph_mgr.get_next_id(), std::make_unique<qsyn::zx::ZXGraph>(std::move(graph.value())));

                    zxgraph_mgr.get()->set_filename(qcir_mgr.get()->get_filename());
                    zxgraph_mgr.get()->add_procedures(qcir_mgr.get()->get_procedures());
                    zxgraph_mgr.get()->add_procedure("QC2ZX");
                }
                return CmdExecResult::done;
            }
            if (to_type == "tensor") {
                spdlog::info("Converting to QCir {} to Tensor {}...", qcir_mgr.focused_id(), tensor_mgr.get_next_id());
                auto tensor = to_tensor(*qcir_mgr.get());

                if (tensor.has_value()) {
                    *tensor = tensor->to_matrix();
                    tensor_mgr.add(tensor_mgr.get_next_id());
                    tensor_mgr.set(std::make_unique<qsyn::tensor::QTensor<double>>(std::move(tensor.value())));

                    tensor_mgr.get()->set_filename(qcir_mgr.get()->get_filename());
                    tensor_mgr.get()->add_procedures(qcir_mgr.get()->get_procedures());
                    tensor_mgr.get()->add_procedure("QC2TS");
                }
                return CmdExecResult::done;
            }
            if (to_type == "tableau") {
                spdlog::info("Converting to QCir {} to Tableau {}...", qcir_mgr.focused_id(), tableau_mgr.get_next_id());
                auto tableau = experimental::to_tableau(*qcir_mgr.get());

                if (tableau.has_value()) {
                    tableau_mgr.add(tableau_mgr.get_next_id(), std::make_unique<experimental::Tableau>(std::move(tableau.value())));

                    tableau_mgr.get()->set_filename(qcir_mgr.get()->get_filename());
                    tableau_mgr.get()->add_procedures(qcir_mgr.get()->get_procedures());
                    tableau_mgr.get()->add_procedure("QC2TABL");
                }
                return CmdExecResult::done;
            }

            spdlog::error("The conversion is not supported yet!!");
            return CmdExecResult::error;
        }};
}

Command convert_from_zx_cmd(zx::ZXGraphMgr& zxgraph_mgr, QCirMgr& qcir_mgr, tensor::TensorMgr& tensor_mgr) {
    return {
        "zx",
        [&](ArgumentParser& parser) {
            parser.description("convert from ZXGraph to other data structures");

            auto subparsers = parser.add_subparsers("to-type")
                                  .required(true);

            auto to_qcir = subparsers.add_parser("qcir")
                               .description("convert from ZXGraph to QCir");

            auto to_tensor = subparsers.add_parser("tensor")
                                 .description("convert from ZXGraph to Tensor");

            to_qcir.add_argument<bool>("-r", "--random")
                .action(store_true)
                .help("Shuffle the neighbors to the extraction frontier, which changes the gadget removal order.");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(zxgraph_mgr)) return CmdExecResult::error;
            auto to_type = parser.get<std::string>("to-type");
            if (to_type == "qcir") {
                if (!is_graph_like(*zxgraph_mgr.get())) {
                    spdlog::error("ZXGraph {} is not extractable because it is not graph-like!!", zxgraph_mgr.focused_id());
                    return CmdExecResult::error;
                }
                zx::ZXGraph target = *zxgraph_mgr.get();
                extractor::Extractor ext(
                    &target,
                    extractor::EXTRACTOR_CONFIG,
                    nullptr,
                    parser.parsed("--random") /*, std::nullopt*/);
                qcir::QCir* result = ext.extract();
                if (result != nullptr) {
                    qcir_mgr.add(qcir_mgr.get_next_id(), std::unique_ptr<qcir::QCir>(result));
                    qcir_mgr.get()->set_filename(zxgraph_mgr.get()->get_filename());
                    qcir_mgr.get()->add_procedures(zxgraph_mgr.get()->get_procedures());
                    if (!extractor::EXTRACTOR_CONFIG.permute_qubits) {
                        spdlog::warn("The extracted circuit is up to a qubit permutation.");
                        spdlog::warn("Remaining permutation information is in ZXGraph id {}.", zxgraph_mgr.get_next_id());
                        zxgraph_mgr.add(zxgraph_mgr.get_next_id(), std::make_unique<zx::ZXGraph>(std::move(target)));
                        zxgraph_mgr.get()->add_procedure("ZX2QC-Unpermuted");
                        qcir_mgr.get()->add_procedure("ZX2QC-Unpermuted");
                    } else
                        qcir_mgr.get()->add_procedure("ZX2QC");

                    assert(std::ranges::all_of(qcir_mgr.get()->get_gates(), [&](auto* gate) { return gate->get_id() == qcir_mgr.get()->get_gate(gate->get_id())->get_id(); }));
                }
                return CmdExecResult::done;
            }
            if (to_type == "tensor") {
                spdlog::info("Converting ZXGraph {} to Tensor {}...", zxgraph_mgr.focused_id(), tensor_mgr.get_next_id());
                auto tensor = qsyn::to_tensor(*zxgraph_mgr.get());

                if (tensor.has_value()) {
                    tensor_mgr.add(tensor_mgr.get_next_id(), std::make_unique<qsyn::tensor::QTensor<double>>(std::move(tensor.value())));

                    tensor_mgr.get()->set_filename(zxgraph_mgr.get()->get_filename());
                    tensor_mgr.get()->add_procedures(zxgraph_mgr.get()->get_procedures());
                    tensor_mgr.get()->add_procedure("ZX2TS");
                }
                return CmdExecResult::done;
            }

            spdlog::error("The conversion is not supported yet!!");
            return CmdExecResult::error;
        }};
}

Command convert_from_tensor_cmd(tensor::TensorMgr& tensor_mgr, QCirMgr& qcir_mgr) {
    return {
        "tensor",
        [&](ArgumentParser& parser) {
            parser.description("convert from Tensor to other data structures");

            auto subparsers = parser.add_subparsers("to-type")
                                  .required(true);

            auto to_qcir = subparsers.add_parser("qcir")
                               .description("convert from Tensor to QCir");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(tensor_mgr)) return CmdExecResult::error;
            auto to_type = parser.get<std::string>("to-type");
            if (to_type == "qcir") {
                spdlog::info("Converting Tensor {} to QCir {}...", tensor_mgr.focused_id(), qcir_mgr.get_next_id());
                auto result = tensor::Decomposer{}.decompose(*tensor_mgr.get());

                if (result) {
                    qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<qcir::QCir>(std::move(*result)));
                    qcir_mgr.get()->add_procedures(tensor_mgr.get()->get_procedures());
                    qcir_mgr.get()->add_procedure("TS2QC");
                    qcir_mgr.get()->set_filename(tensor_mgr.get()->get_filename());
                }

                return CmdExecResult::done;
            }
            spdlog::error("The conversion is not supported yet!!");
            return CmdExecResult::error;
        }};
}

Command convert_from_tableau_cmd(experimental::TableauMgr& tableau_mgr, qcir::QCirMgr& qcir_mgr) {
    return {
        "tableau",
        [&](ArgumentParser& parser) {
            parser.description("convert from Tableau to other data structures");

            auto subparsers = parser.add_subparsers("to-type")
                                  .required(true);

            auto to_qcir = subparsers.add_parser("qcir")
                               .description("convert from Tableau to QCir");

            to_qcir.add_argument<std::string>("-c", "--clifford")
                .constraint(choices_allow_prefix({"hopt", "ag", "hstair"}))
                .default_value("hopt")
                .help("specify the Clifford synthesis strategy (default: hopt).");

            to_qcir.add_argument<std::string>("-r", "--rotation")
                .constraint(choices_allow_prefix({"naive", "graysynth", "gstair", "mst"}))
                .default_value("naive")
                .help("specify the rotation synthesis strategy (default: naive).");
        },
        [&](ArgumentParser const& parser) {
            using namespace dvlab::str;
            if (!dvlab::utils::mgr_has_data(tableau_mgr)) return CmdExecResult::error;
            auto to_type = parser.get<std::string>("to-type");
            if (to_type == "qcir") {
                auto const clifford_strategy = std::invoke([&]() -> std::unique_ptr<experimental::StabilizerTableauSynthesisStrategy> {
                    auto const clifford_strategy_str = parser.get<std::string>("--clifford");
                    if (is_prefix_of(clifford_strategy_str, "hopt")) return std::make_unique<experimental::HOptSynthesisStrategy>();
                    if (is_prefix_of(clifford_strategy_str, "ag")) return std::make_unique<experimental::AGSynthesisStrategy>();
                    if (is_prefix_of(clifford_strategy_str, "hstair")) return std::make_unique<experimental::HOptSynthesisStrategy>(experimental::HOptSynthesisStrategy::Mode::staircase);
                    DVLAB_UNREACHABLE("Invalid clifford strategy!!");
                    return nullptr;
                });

                auto const rotation_strategy = std::invoke([&]() -> std::unique_ptr<experimental::PauliRotationsSynthesisStrategy> {
                    auto const rotation_strategy_str = parser.get<std::string>("--rotation");
                    if (is_prefix_of(rotation_strategy_str, "naive")) return std::make_unique<experimental::NaivePauliRotationsSynthesisStrategy>();
                    if (is_prefix_of(rotation_strategy_str, "graysynth")) return std::make_unique<experimental::GraySynthStrategy>();
                    if (is_prefix_of(rotation_strategy_str, "gstair")) return std::make_unique<experimental::GraySynthStrategy>(experimental::GraySynthStrategy::Mode::staircase);
                    if (is_prefix_of(rotation_strategy_str, "mst")) return std::make_unique<experimental::MstSynthesisStrategy>();
                    DVLAB_UNREACHABLE("Invalid rotation strategy!!");
                    return nullptr;
                });

                spdlog::info("Converting to Tableau {} to QCir {}...", tableau_mgr.focused_id(), qcir_mgr.get_next_id());
                auto qcir = experimental::to_qcir(*tableau_mgr.get(), *clifford_strategy, *rotation_strategy);

                if (qcir.has_value()) {
                    qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<qcir::QCir>(std::move(qcir.value())));

                    qcir_mgr.get()->set_filename(tableau_mgr.get()->get_filename());
                    qcir_mgr.get()->add_procedures(tableau_mgr.get()->get_procedures());
                    qcir_mgr.get()->add_procedure("TABL2QC");
                }

                return CmdExecResult::done;
            }

            spdlog::error("The conversion is not supported yet!!");
            return CmdExecResult::error;
        }};
}

Command conversion_cmd(QCirMgr& qcir_mgr, qsyn::tensor::TensorMgr& tensor_mgr, qsyn::zx::ZXGraphMgr& zxgraph_mgr, experimental::TableauMgr& tableau_mgr) {
    auto cmd = dvlab::Command{
        "convert",
        [&](ArgumentParser& parser) {
            parser.description("conversion from one data structure to another");
            auto subparsers = parser.add_subparsers("from-type").required(true);
        },
        [&](ArgumentParser const& /*parser*/) {
            return CmdExecResult::error;
        }};

    cmd.add_subcommand("from-type", convert_from_qcir_cmd(qcir_mgr, zxgraph_mgr, tensor_mgr, tableau_mgr));
    cmd.add_subcommand("from-type", convert_from_zx_cmd(zxgraph_mgr, qcir_mgr, tensor_mgr));
    cmd.add_subcommand("from-type", convert_from_tensor_cmd(tensor_mgr, qcir_mgr));
    cmd.add_subcommand("from-type", convert_from_tableau_cmd(tableau_mgr, qcir_mgr));

    return cmd;
}

Command sk_decompose_cmd(qsyn::tensor::TensorMgr& tensor_mgr, QCirMgr& qcir_mgr) {
    return {"sk-decompose",
            [&](ArgumentParser& parser) {
                parser.description("decompose the tensor by SK-algorithm");
                parser.add_argument<size_t>("-d", "--depth")
                    .required(true)
                    .help("the depth of the gate list");

                parser.add_argument<size_t>("-r", "--recursion")
                    .required(true)
                    .help("the recursion times of Solovay-Kitaev algorithm");
            },
            // NOTE - Check the function solovay_kitaev_decompose
            [&](ArgumentParser const& parser) {
                tensor::SolovayKitaev decomposer(parser.get<size_t>("--depth"), parser.get<size_t>("--recursion"));
                spdlog::info("Decomposing Tensor {} to QCir {} by Solovay-Kitaev algorithm...", tensor_mgr.focused_id(), qcir_mgr.get_next_id());
                auto result = decomposer.solovay_kitaev_decompose(*tensor_mgr.get());

                if (result) {
                    qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<qcir::QCir>(std::move(*result)));
                    qcir_mgr.get()->add_procedures(tensor_mgr.get()->get_procedures());
                    qcir_mgr.get()->add_procedure("Solovay-Kitaev");
                    qcir_mgr.get()->set_filename(tensor_mgr.get()->get_filename());
                }

                return CmdExecResult::done;
            }};
}

bool add_conversion_cmds(dvlab::CommandLineInterface& cli, QCirMgr& qcir_mgr, qsyn::tensor::TensorMgr& tensor_mgr, qsyn::zx::ZXGraphMgr& zxgraph_mgr, experimental::TableauMgr& tableau_mgr) {
    if (!(cli.add_command(conversion_cmd(qcir_mgr, tensor_mgr, zxgraph_mgr, tableau_mgr)) &&
          cli.add_command(sk_decompose_cmd(tensor_mgr, qcir_mgr)) &&
          cli.add_alias("qc2zx", "convert qcir zx") &&
          cli.add_alias("qc2ts", "convert qcir tensor") &&
          cli.add_alias("zx2ts", "convert zx tensor") &&
          cli.add_alias("zx2qc", "convert zx qcir") &&
          cli.add_alias("ts2qc", "convert tensor qcir") &&
          cli.add_alias("qc2tabl", "convert qcir tableau") &&
          cli.add_alias("tabl2qc", "convert tableau qcir"))) {
        fmt::println(stderr, "Registering \"conversion\" commands fails... exiting");
        return false;
    }
    return true;
}

}  // namespace qsyn
