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
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/phase.hpp"
#include "util/text_format.hpp"
// #include "./tensor_decompose.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::tensor {

ArgType<size_t>::ConstraintType valid_tensor_id(TensorMgr const& tensor_mgr) {
    return [&](size_t const& id) {
        if (tensor_mgr.is_id(id)) return true;
        spdlog::error("Cannot find tensor with ID {}!!", id);
        return false;
    };
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
                    fmt::println("{}", *tensor_mgr.find_by_id(parser.get<size_t>("id")));
                } else {
                    fmt::println("{}", *tensor_mgr.get());
                }
                return CmdExecResult::done;
            }};
}

Command tensor_read_cmd(TensorMgr& tensor_mgr){
    return {"read",
        [&](ArgumentParser& parser) {
            parser.description("read a matrix(.csv) and construct the corresponding tensor");
            parser.add_argument<std::string>("filepath")
                .help("the filepath to matrix file.  Supported extension: .csv");
            parser.add_argument<bool>("-r", "--replace")
                .action(store_true)
                .help("if specified, replace the current tensor; otherwise store to a new one");
        },
        [&tensor_mgr](ArgumentParser const& parser) {
            QTensor<double> *buffer_q_tensor = new QTensor<double>();
            auto filepath = parser.get<std::string>("filepath");
            auto replace  = parser.get<bool>("--replace");

            if (!buffer_q_tensor->tensor_read(filepath)) {
                spdlog::error("the format in \"{}\" has something wrong!!", filepath);
                return CmdExecResult::error;
            }
            auto unique_ptr_qtensor = std::make_unique<QTensor<double>>(std::move(*buffer_q_tensor));
            if (tensor_mgr.empty() || !replace) {              
                tensor_mgr.add(tensor_mgr.get_next_id(), std::move(unique_ptr_qtensor));
            } else {
                tensor_mgr.set(std::move(unique_ptr_qtensor));
            }

            return CmdExecResult::done;
        }};
}

Command tensor_decompose_cmd(TensorMgr& tensor_mgr) {
    return {"decompose",
            [&](ArgumentParser& parser) {
                parser.description("Decompose the unitary matrix into multi two level matrix");

                parser.add_argument<std::string>("filepath")
                    // .constraint(valid_tensor_id(tensor_mgr))
                    // .nargs(NArgsOption::optional)
                    .help("the output path of the .qasm");
            },
            [&](ArgumentParser const& parser) {
                auto filepath = parser.get<std::string>("filepath");
                QTensor<double>* tensor = tensor_mgr.get();
                decompose(*tensor, filepath);
                // if (parser.parsed("id")) {
                //     QTensor<double>* tensor = tensor_mgr.find_by_id(parser.get<size_t>("id"));
                //     decompose(*tensor);
                // } else {
                //     
                //     decompose(*tensor);
                // }
                return CmdExecResult::done;
            }};
}

// Command tensor_check_cmd(TensorMgr& tensor_mgr) {
//     return {"decompose",
//             [&](ArgumentParser& parser) {
//                 parser.description("Decompose the unitary matrix into multi two level matrix");

//                 parser.add_argument<size_t>("id")
//                     .constraint(valid_tensor_id(tensor_mgr))
//                     .nargs(NArgsOption::optional)
//                     .help("if specified, decompose the tensor with the ID");
//             },
//             [&](ArgumentParser const& parser) {
//                 if (parser.parsed("id")) {
//                     QTensor<double>* tensor = tensor_mgr.find_by_id(parser.get<size_t>("id"));
//                     decompose(*tensor);
//                 } else {
//                     QTensor<double>* tensor = tensor_mgr.get();
//                     QTensor<double> t;
//                     t = {
//                         {0.353553 + 0.i, 0. + 0.i, -0.612372 + 0.i, 0.707107 + 0.i},
//                         {0. + -0.866025i, 0. + 0.i, 0. - 0.5i, 0. + 0.i},
//                         {0. + 0.i, 0. + 1.i, 0. + 0.i, 0. + 0.i}, // test
//                         {-0.353553 + 0.i, 0. + 0.i, 0.612372 + 0.i, 0.707107 + 0.i},
//                     };
//                     std::cout << "is equivalent: " << is_equivalent(tensor, *t, 1e-6);
//                 }
//                 return CmdExecResult::done;
//             }};
// }

Command tensor_adjoint_cmd(TensorMgr& tensor_mgr) {
    return {"adjoint",
            [&](ArgumentParser& parser) {
                parser.description("transform the tensor to its adjoint");

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
                parser.add_argument<double>("-e", "--epsilon")
                    .metavar("eps")
                    .default_value(1e-6)
                    .help("output \"equivalent\" if the Frobenius inner product is at least than 1 - eps (default: 1e-6)");
                parser.add_argument<bool>("-s", "--strict")
                    .help("requires global scaling factor to be 1")
                    .action(store_true);
            },
            [&](ArgumentParser const& parser) {
                auto ids    = parser.get<std::vector<size_t>>("ids");
                auto eps    = parser.get<double>("--epsilon");
                auto strict = parser.get<bool>("--strict");

                QTensor<double>* tensor1 = nullptr;
                QTensor<double>* tensor2 = nullptr;
                if (ids.size() == 2) {
                    tensor1 = tensor_mgr.find_by_id(ids[0]);
                    tensor2 = tensor_mgr.find_by_id(ids[1]);
                } else {
                    tensor1 = tensor_mgr.get();
                    tensor2 = tensor_mgr.find_by_id(ids[0]);
                }

                bool equiv       = is_equivalent(*tensor1, *tensor2, eps);
                auto const norm  = global_norm(*tensor1, *tensor2);
                auto const phase = global_phase(*tensor1, *tensor2);

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
    using namespace dvlab::utils;
    auto cmd = mgr_root_cmd(tensor_mgr);
    cmd.add_subcommand(mgr_list_cmd(tensor_mgr));
    cmd.add_subcommand(tensor_print_cmd(tensor_mgr));
    cmd.add_subcommand(mgr_checkout_cmd(tensor_mgr));
    cmd.add_subcommand(mgr_delete_cmd(tensor_mgr));
    cmd.add_subcommand(tensor_adjoint_cmd(tensor_mgr));
    cmd.add_subcommand(tensor_equivalence_check_cmd(tensor_mgr));
    cmd.add_subcommand(tensor_decompose_cmd(tensor_mgr));
    cmd.add_subcommand(tensor_read_cmd(tensor_mgr));
    // cmd.add_subcommand(tensor_check_cmd(tensor_mgr));

    return cmd;
}

bool add_tensor_cmds(dvlab::CommandLineInterface& cli, TensorMgr& tensor_mgr) {
    if (!cli.add_command(tensor_cmd(tensor_mgr))) {
        spdlog::error("Registering \"tensor\" commands fails... exiting");
        return false;
    }
    return true;
}

}  // namespace qsyn::tensor
