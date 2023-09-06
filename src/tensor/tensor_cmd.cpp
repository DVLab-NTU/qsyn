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

using namespace std;

TensorMgr TENSOR_MGR{"Tensor"};

using namespace argparse;

Command tensor_mgr_reset_cmd();
Command tensor_mgr_print_cmd();
Command tensor_adjoint_cmd();
Command tensor_print_cmd();
Command tensor_equivalence_check_cmd();

bool add_tensor_cmds() {
    if (!(
            CLI.add_command(tensor_mgr_reset_cmd()) &&
            CLI.add_command(tensor_mgr_print_cmd()) &&
            CLI.add_command(tensor_adjoint_cmd()) &&
            CLI.add_command(tensor_print_cmd()) &&
            CLI.add_command(tensor_equivalence_check_cmd()))) {
        cerr << "Registering \"tensor\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType VALID_TENSOR_ID =
    [](size_t const& id) {
        if (TENSOR_MGR.is_id(id)) return true;
        cerr << "Error: Can't find tensor with ID " << id << "!!" << endl;
        return false;
    };

Command tensor_mgr_reset_cmd() {
    return {"tsreset",
            [](ArgumentParser& parser) {
                parser.description("reset the tensor manager");
            },
            [](ArgumentParser const& parser) {
                TENSOR_MGR.reset();
                return CmdExecResult::done;
            }};
}

Command tensor_mgr_print_cmd() {
    return {"tsprint",
            [](ArgumentParser& parser) {
                parser.description("print info about Tensors");
                auto mutex = parser.add_mutually_exclusive_group().required(false);

                mutex.add_argument<bool>("-focus")
                    .action(store_true)
                    .help("print the info of the Tensor in focus");
                mutex.add_argument<bool>("-list")
                    .action(store_true)
                    .help("print a list of Tensors");
            },
            [](ArgumentParser const& parser) {
                if (parser.parsed("-focus"))
                    TENSOR_MGR.print_focus();
                else if (parser.parsed("-list"))
                    TENSOR_MGR.print_list();
                else
                    TENSOR_MGR.print_manager();

                return CmdExecResult::done;
            }};
}

Command tensor_print_cmd() {
    return {"tstprint",
            [](ArgumentParser& parser) {
                parser.description("print info of Tensor");

                parser.add_argument<size_t>("id")
                    .constraint(VALID_TENSOR_ID)
                    .nargs(NArgsOption::optional)
                    .help("if specified, print the tensor with the ID");
            },
            [](ArgumentParser const& parser) {
                if (parser.parsed("id")) {
                    cout << *TENSOR_MGR.find_by_id(parser.get<size_t>("id")) << endl;
                } else {
                    cout << *TENSOR_MGR.get() << endl;
                }

                return CmdExecResult::done;
            }};
}

Command tensor_adjoint_cmd() {
    return {"tsadjoint",
            [](ArgumentParser& parser) {
                parser.description("adjoint the specified tensor");

                parser.add_argument<size_t>("id")
                    .constraint(VALID_TENSOR_ID)
                    .nargs(NArgsOption::optional)
                    .help("the ID of the tensor");
            },
            [](ArgumentParser const& parser) {
                if (parser.parsed("id")) {
                    TENSOR_MGR.find_by_id(parser.get<size_t>("id"))->adjoint();
                } else {
                    TENSOR_MGR.get()->adjoint();
                }
                return CmdExecResult::done;
            }};
}
Command tensor_equivalence_check_cmd() {
    return {"tsequiv",
            [](ArgumentParser& parser) {
                parser.description("check the equivalency of two stored tensors");

                parser.add_argument<size_t>("ids")
                    .nargs(1, 2)
                    .constraint(VALID_TENSOR_ID)
                    .help("Compare the two tensors. If only one is specified, compare with the tensor on focus");
                parser.add_argument<double>("-epsilon")
                    .metavar("eps")
                    .default_value(1e-6)
                    .help("output \"equivalent\" if the Frobenius inner product is at least than 1 - eps (default: 1e-6)");
                parser.add_argument<bool>("-strict")
                    .help("requires global scaling factor to be 1")
                    .action(store_true);
            },
            [](ArgumentParser const& parser) {
                auto ids = parser.get<vector<size_t>>("ids");
                auto eps = parser.get<double>("-epsilon");
                auto strict = parser.get<bool>("-strict");

                QTensor<double>* tensor1;
                QTensor<double>* tensor2;
                if (ids.size() == 2) {
                    tensor1 = TENSOR_MGR.find_by_id(ids[0]);
                    tensor2 = TENSOR_MGR.find_by_id(ids[1]);
                } else {
                    tensor1 = TENSOR_MGR.get();
                    tensor2 = TENSOR_MGR.find_by_id(ids[0]);
                }

                bool equiv = is_equivalent(*tensor1, *tensor2, eps);
                double norm = global_norm<double>(*tensor1, *tensor2);
                Phase phase = global_phase<double>(*tensor1, *tensor2);

                if (strict) {
                    if (norm > 1 + eps || norm < 1 - eps || phase != Phase(0)) {
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
