#include <spdlog/spdlog.h>

#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"
#include "qcir/optimizer/optimizer.hpp"
#include "qcir/qcir.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

extern bool stop_requested();

namespace qsyn::qcir {

Command qcir_to_basic_cmd(QCirMgr& qcir_mgr) {
    return {"to-basic",
            [&](ArgumentParser& parser) {
                parser.description("Convert the QCir to use only basic gates");
            },
            [&](ArgumentParser const& /* parser */) -> CmdExecResult {
                auto result = to_basic_gates(*qcir_mgr.get());
                if (!result.has_value()) {
                    spdlog::error("Failed to convert to basic gates!!");
                    return CmdExecResult::error;
                }
                *qcir_mgr.get() = std::move(*result);
                return CmdExecResult::done;
            }};
}

}  // namespace qsyn::qcir
