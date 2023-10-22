/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define conversion among QCir, ZXGraph, and Tensor ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./conversion_cmd.hpp"

#include <spdlog/spdlog.h>

#include "./qcir_to_tensor.hpp"
#include "./qcir_to_zxgraph.hpp"
#include "./zxgraph_to_tensor.hpp"
#include "cli/cli.hpp"
#include "qcir/qcir_mgr.hpp"
#include "tensor/tensor_mgr.hpp"
#include "zx/zx_cmd.hpp"

using namespace dvlab::argparse;

using dvlab::Command, dvlab::CmdExecResult;

namespace qsyn {

bool valid_decomposition_mode(size_t const& val) {
    if (val <= 4) return true;
    std::cerr << "Error: decomposition Mode " << val << " is not valid!!\n";
    return false;
};

Command qcir_to_zx_cmd(QCirMgr& qcir_mgr, qsyn::zx::ZXGraphMgr& zxgraph_mgr) {
    return {"qcir2zx",
            [](ArgumentParser& parser) {
                parser.description("convert QCir to ZXGraph");

                parser.add_argument<size_t>("decomp_mode")
                    .default_value(0)
                    .constraint(valid_decomposition_mode)
                    .help("specify the decomposition mode (default: 0). The higher the number, the more aggressive the decomposition is.");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                spdlog::info("Converting to QCir {} to ZXGraph {}...", qcir_mgr.focused_id(), zxgraph_mgr.get_next_id());
                auto g = to_zxgraph(*qcir_mgr.get(), parser.get<size_t>("decomp_mode"));

                if (g.has_value()) {
                    zxgraph_mgr.add(zxgraph_mgr.get_next_id(), std::make_unique<qsyn::zx::ZXGraph>(std::move(g.value())));

                    zxgraph_mgr.get()->set_filename(qcir_mgr.get()->get_filename());
                    zxgraph_mgr.get()->add_procedures(qcir_mgr.get()->get_procedures());
                    zxgraph_mgr.get()->add_procedure("QC2ZX");
                }

                return CmdExecResult::done;
            }};
}

Command qcir_to_tensor_cmd(QCirMgr& qcir_mgr, qsyn::tensor::TensorMgr& tensor_mgr) {
    return {"qcir2tensor",
            [](ArgumentParser& parser) {
                parser.description("convert QCir to tensor");
            },
            [&](ArgumentParser const&) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                spdlog::info("Converting to QCir {} to tensor {}...", qcir_mgr.focused_id(), tensor_mgr.get_next_id());
                auto tensor = to_tensor(*qcir_mgr.get());

                if (tensor.has_value()) {
                    tensor_mgr.add(tensor_mgr.get_next_id());
                    tensor_mgr.set(std::make_unique<qsyn::tensor::QTensor<double>>(std::move(tensor.value())));

                    tensor_mgr.get()->set_filename(qcir_mgr.get()->get_filename());
                    tensor_mgr.get()->add_procedures(qcir_mgr.get()->get_procedures());
                    tensor_mgr.get()->add_procedure("QC2TS");
                }

                return CmdExecResult::done;
            }};
}

Command zxgraph_to_tensor_cmd(qsyn::zx::ZXGraphMgr& zxgraph_mgr, qsyn::tensor::TensorMgr& tensor_mgr) {
    return {"zx2tensor",
            [&](ArgumentParser& parser) {
                parser.description("convert ZXGraph to tensor");

                parser.add_argument<size_t>("-zx")
                    .metavar("id")
                    .constraint(qsyn::zx::valid_zxgraph_id(zxgraph_mgr))
                    .help("the ID of the ZXGraph to be converted. If not specified, the focused ZXGraph is used");

                parser.add_argument<size_t>("-ts")
                    .metavar("id")
                    .help("the ID of the target tensor. If not specified, an ID is automatically assigned");

                parser.add_argument<bool>("-replace")
                    .action(store_true)
                    .help("replace the target tensor if the tensor ID is occupied");
            },
            [&](ArgumentParser const& parser) {
                if (!qsyn::zx::zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                auto zx_id = parser.parsed("-zx") ? parser.get<size_t>("-zx") : zxgraph_mgr.focused_id();
                auto zx    = zxgraph_mgr.find_by_id(zx_id);

                auto ts_id = parser.parsed("-ts") ? parser.get<size_t>("-ts") : tensor_mgr.get_next_id();

                if (tensor_mgr.is_id(ts_id) && !parser.parsed("-replace")) {
                    spdlog::error("Tensor {} already exists!! Specify `-Replace` if you intend to replace the current one.", ts_id);
                    return CmdExecResult::error;
                }
                spdlog::info("Converting ZXGraph {} to Tensor {}...", zx_id, ts_id);
                auto tensor = qsyn::to_tensor(*zx);

                if (tensor.has_value()) {
                    if (!tensor_mgr.is_id(ts_id)) {
                        tensor_mgr.add(ts_id, std::make_unique<qsyn::tensor::QTensor<double>>(std::move(tensor.value())));
                    } else {
                        tensor_mgr.checkout(ts_id);
                        tensor_mgr.set(std::make_unique<qsyn::tensor::QTensor<double>>(std::move(tensor.value())));
                    }

                    tensor_mgr.get()->set_filename(zx->get_filename());
                    tensor_mgr.get()->add_procedures(zx->get_procedures());
                    tensor_mgr.get()->add_procedure("ZX2TS");
                }

                return CmdExecResult::done;
            }};
}

bool add_conversion_cmds(dvlab::CommandLineInterface& cli, QCirMgr& qcir_mgr, qsyn::tensor::TensorMgr& tensor_mgr, qsyn::zx::ZXGraphMgr& zxgraph_mgr) {
    if (!(cli.add_command(qcir_to_zx_cmd(qcir_mgr, zxgraph_mgr)) &&
          cli.add_command(zxgraph_to_tensor_cmd(zxgraph_mgr, tensor_mgr)) &&
          cli.add_command(qcir_to_tensor_cmd(qcir_mgr, tensor_mgr)) &&
          cli.add_alias("qc2zx", "qcir2zx") &&
          cli.add_alias("qc2ts", "qcir2tensor") &&
          cli.add_alias("zx2ts", "zx2tensor"))) {
        fmt::println(stderr, "Registering \"conversion\" commands fails... exiting");
        return false;
    }
    return true;
}

}  // namespace qsyn
