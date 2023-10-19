/****************************************************************************
  PackageName  [ tensor ]
  Synopsis     [ Define tensor package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <string>

#include "./tensor_mgr.hpp"
#include "cli/cli.hpp"
#include "util/phase.hpp"
#include "util/text_format.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::tensor {

ArgType<size_t>::ConstraintType valid_tensor_id(TensorMgr const& tensor_mgr) {
    return [&](size_t const& id) {
        if (tensor_mgr.is_id(id)) return true;
        std::cerr << "Error: Can't find tensor with ID " << id << "!!" << std::endl;
        return false;
    };
}

Command tensor_clear_cmd(TensorMgr& tensor_mgr) {
    return {"clear",
            [](ArgumentParser& parser) {
                parser.description("clear the tensor manager");
            },
            [&](ArgumentParser const& /*parser*/) {
                tensor_mgr.clear();
                return CmdExecResult::done;
            }};
}

Command tensor_list_cmd(TensorMgr& tensor_mgr) {
    return {"list",
            [](ArgumentParser& parser) {
                parser.description("list info about Tensors");
            },
            [&](ArgumentParser const& /*unused*/) {
                tensor_mgr.print_list();

                return CmdExecResult::done;
            }};
}

Command tensor_print_cmd(TensorMgr& tensor_mgr) {
    return {"print",
            [&](ArgumentParser& parser) {
                parser.description("print info of Tensor");

                parser.add_argument<size_t>("id")
                    .constraint(valid_tensor_id(tensor_mgr))
                    .nargs(NArgsOption::optional)
                    .help("if specified, print the tensor with the ID");
            },
            [&](ArgumentParser const& parser) {
                if (parser.parsed("id")) {
                    std::cout << *tensor_mgr.find_by_id(parser.get<size_t>("id")) << std::endl;
                } else {
                    std::cout << *tensor_mgr.get() << std::endl;
                }

                return CmdExecResult::done;
            }};
}

Command tensor_adjoint_cmd(TensorMgr& tensor_mgr) {
    return {"adjoint",
            [&](ArgumentParser& parser) {
                parser.description("adjoint the specified tensor");

                parser.add_argument<size_t>("id")
                    .constraint(valid_tensor_id(tensor_mgr))
                    .nargs(NArgsOption::optional)
                    .help("the ID of the tensor");
            },
            [&](ArgumentParser const& parser) {
                if (parser.parsed("id")) {
                    tensor_mgr.find_by_id(parser.get<size_t>("id"))->adjoint();
                } else {
                    tensor_mgr.get()->adjoint();
                }
                return CmdExecResult::done;
            }};
}
Command tensor_equivalence_check_cmd(TensorMgr& tensor_mgr) {
    return {"equiv",
            [&](ArgumentParser& parser) {
                parser.description("check the equivalency of two stored tensors");

                parser.add_argument<size_t>("ids")
                    .nargs(1, 2)
                    .constraint(valid_tensor_id(tensor_mgr))
                    .help("Compare the two tensors. If only one is specified, compare with the tensor on focus");
                parser.add_argument<double>("-epsilon")
                    .metavar("eps")
                    .default_value(1e-6)
                    .help("output \"equivalent\" if the Frobenius inner product is at least than 1 - eps (default: 1e-6)");
                parser.add_argument<bool>("-strict")
                    .help("requires global scaling factor to be 1")
                    .action(store_true);
            },
            [&](ArgumentParser const& parser) {
                auto ids    = parser.get<std::vector<size_t>>("ids");
                auto eps    = parser.get<double>("-epsilon");
                auto strict = parser.get<bool>("-strict");

                QTensor<double>* tensor1;
                QTensor<double>* tensor2;
                if (ids.size() == 2) {
                    tensor1 = tensor_mgr.find_by_id(ids[0]);
                    tensor2 = tensor_mgr.find_by_id(ids[1]);
                } else {
                    tensor1 = tensor_mgr.get();
                    tensor2 = tensor_mgr.find_by_id(ids[0]);
                }

                bool equiv         = is_equivalent(*tensor1, *tensor2, eps);
                double norm        = global_norm<double>(*tensor1, *tensor2);
                dvlab::Phase phase = global_phase<double>(*tensor1, *tensor2);

                if (strict) {
                    if (norm > 1 + eps || norm < 1 - eps || phase != dvlab::Phase(0)) {
                        equiv = false;
                    }
                }
                using namespace dvlab;
                if (equiv) {
                    fmt::println("{}", fmt_ext::styled_if_ansi_supported("Equivalent", fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
                    fmt::println("- Global Norm : {:.6}", norm);
                    fmt::println("- Global Phase: {}", phase);
                } else {
                    fmt::println("{}", fmt_ext::styled_if_ansi_supported("Not Equivalent", fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
                }

                return CmdExecResult::done;
            }};
}

Command tensor_cmd(TensorMgr& tensor_mgr) {
    auto cmd = Command{"tensor",
                       [&](ArgumentParser& parser) {
                           parser.description("tensor commands");
                       },
                       [&](ArgumentParser const& /*parser*/) {
                           tensor_mgr.print_manager();
                           return CmdExecResult::done;
                       }};
    cmd.add_subcommand(tensor_clear_cmd(tensor_mgr));
    cmd.add_subcommand(tensor_list_cmd(tensor_mgr));
    cmd.add_subcommand(tensor_print_cmd(tensor_mgr));
    cmd.add_subcommand(tensor_adjoint_cmd(tensor_mgr));
    cmd.add_subcommand(tensor_equivalence_check_cmd(tensor_mgr));

    return cmd;
}

bool add_tensor_cmds(dvlab::CommandLineInterface& cli, TensorMgr& tensor_mgr) {
    if (!(cli.add_command(tensor_cmd(tensor_mgr)))) {
        std::cerr << "Registering \"tensor\" commands fails... exiting" << std::endl;
        return false;
    }
    return true;
}

}  // namespace qsyn::tensor
