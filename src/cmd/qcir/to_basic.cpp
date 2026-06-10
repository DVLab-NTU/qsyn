#include <spdlog/spdlog.h>

#include <optional>
#include <string>

#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"
#include "qcir/qcir.hpp"
#include "util/dvlab_string.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

extern bool stop_requested();

namespace qsyn::qcir {

namespace {

std::optional<CcDecomposition> parse_cc_decomposition(std::string const& value) {
    if (dvlab::str::is_prefix_of(value, "rust") || value == "r") {
        return CcDecomposition::Rust;
    }
    if (dvlab::str::is_prefix_of(value, "cpp") || dvlab::str::is_prefix_of(value, "c++") || value == "c") {
        return CcDecomposition::Cpp;
    }
    return std::nullopt;
}

}  // namespace

Command qcir_to_basic_cmd(QCirMgr& qcir_mgr) {
    return {"to-basic",
            [&](ArgumentParser& parser) {
                parser.description("Convert the QCir to use only basic gates");

                parser.add_argument<std::string>("-d", "--decomp")
                    .default_value("")
                    .help("CCX/CCZ decomposition: r/rust or c/cpp (default: c++)");
            },
            [&](ArgumentParser const& parser) -> CmdExecResult {
                auto decomp = CcDecomposition::Cpp;
                auto const decomp_str = parser.get<std::string>("-d");
                if (!decomp_str.empty()) {
                    auto const parsed = parse_cc_decomposition(decomp_str);
                    if (!parsed.has_value()) {
                        spdlog::error("Invalid --decomp value '{}'; use r/rust or c/cpp", decomp_str);
                        return CmdExecResult::error;
                    }
                    decomp = *parsed;
                }

                auto result = to_basic_gates(*qcir_mgr.get(), decomp);
                if (!result.has_value()) {
                    spdlog::error("Failed to convert to basic gates!!");
                    return CmdExecResult::error;
                }
                *qcir_mgr.get() = std::move(*result);
                return CmdExecResult::done;
            }};
}

}  // namespace qsyn::qcir
