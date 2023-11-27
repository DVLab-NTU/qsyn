/****************************************************************************
  PackageName  [ qsyn ]
  Synopsis     [ Define qsynrc functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <filesystem>

#include "cli/cli.hpp"
#include "device/device_mgr.hpp"
#include "qcir/qcir_mgr.hpp"
#include "tensor/tensor_mgr.hpp"
#include "zx/zxgraph_mgr.hpp"

namespace qsyn {

bool read_qsynrc_file(dvlab::CommandLineInterface& cli, std::filesystem::path qsynrc_path);
bool initialize_qsyn(dvlab::CommandLineInterface& cli, qsyn::device::DeviceMgr& device_mgr,
                     qsyn::qcir::QCirMgr& qcir_mgr, qsyn::tensor::TensorMgr& tensor_mgr,
                     qsyn::zx::ZXGraphMgr& zxgraph_mgr);
dvlab::argparse::ArgumentParser get_qsyn_parser(std::string_view const prog_name);

}  // namespace qsyn
