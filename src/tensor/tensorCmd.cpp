/****************************************************************************
  FileName     [ tensorCmd.cpp ]
  PackageName  [ tensor ]
  Synopsis     [ Define tensor package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <string>

#include "./tensorMgr.hpp"
#include "cli/cli.hpp"
#include "util/phase.hpp"
#include "util/textFormat.hpp"

using namespace std;

TensorMgr tensorMgr{"Tensor"};

using namespace ArgParse;

Command TensorMgrResetCmd();
Command TensorMgrPrintCmd();
Command TensorAdjointCmd();
Command TensorPrintCmd();
Command TensorEquivalenceCmd();

bool initTensorCmd() {
    if (!(
            cli.registerCommand(TensorMgrResetCmd()) &&
            cli.registerCommand(TensorMgrPrintCmd()) &&
            cli.registerCommand(TensorAdjointCmd()) &&
            cli.registerCommand(TensorPrintCmd()) &&
            cli.registerCommand(TensorEquivalenceCmd()))) {
        cerr << "Registering \"tensor\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType validTensorId =
    [](size_t const& id) {
        if (tensorMgr.isID(id)) return true;
        cerr << "Error: Can't find tensor with ID " << id << "!!" << endl;
        return false;
    };

Command TensorMgrResetCmd() {
    return {"tsreset",
            [](ArgumentParser& parser) {
                parser.description("reset the tensor manager");
            },
            [](ArgumentParser const& parser) {
                tensorMgr.reset();
                return CmdExecResult::DONE;
            }};
}

Command TensorMgrPrintCmd() {
    return {"tsprint",
            [](ArgumentParser& parser) {
                parser.description("print info about Tensors");
                auto mutex = parser.addMutuallyExclusiveGroup().required(false);

                mutex.addArgument<bool>("-focus")
                    .action(storeTrue)
                    .help("print the info of the Tensor in focus");
                mutex.addArgument<bool>("-list")
                    .action(storeTrue)
                    .help("print a list of Tensors");
            },
            [](ArgumentParser const& parser) {
                if (parser.parsed("-focus"))
                    tensorMgr.printFocus();
                else if (parser.parsed("-list"))
                    tensorMgr.printList();
                else
                    tensorMgr.printManager();

                return CmdExecResult::DONE;
            }};
}

Command TensorPrintCmd() {
    return {"tstprint",
            [](ArgumentParser& parser) {
                parser.description("print info of Tensor");

                parser.addArgument<size_t>("id")
                    .constraint(validTensorId)
                    .nargs(NArgsOption::OPTIONAL)
                    .help("if specified, print the tensor with the ID");
            },
            [](ArgumentParser const& parser) {
                if (parser.parsed("id")) {
                    cout << *tensorMgr.findByID(parser.get<size_t>("id")) << endl;
                } else {
                    cout << *tensorMgr.get() << endl;
                }

                return CmdExecResult::DONE;
            }};
}

Command TensorAdjointCmd() {
    return {"tsadjoint",
            [](ArgumentParser& parser) {
                parser.description("adjoint the specified tensor");

                parser.addArgument<size_t>("id")
                    .constraint(validTensorId)
                    .nargs(NArgsOption::OPTIONAL)
                    .help("the ID of the tensor");
            },
            [](ArgumentParser const& parser) {
                if (parser.parsed("id")) {
                    tensorMgr.findByID(parser.get<size_t>("id"))->adjoint();
                } else {
                    tensorMgr.get()->adjoint();
                }
                return CmdExecResult::DONE;
            }};
}
Command TensorEquivalenceCmd() {
    return {"tsequiv",
            [](ArgumentParser& parser) {
                parser.description("check the equivalency of two stored tensors");

                parser.addArgument<size_t>("ids")
                    .nargs(1, 2)
                    .constraint(validTensorId)
                    .help("Compare the two tensors. If only one is specified, compare with the tensor on focus");
                parser.addArgument<double>("-epsilon")
                    .metavar("eps")
                    .defaultValue(1e-6)
                    .help("output \"equivalent\" if the Frobenius inner product is at least than 1 - eps (default: 1e-6)");
                parser.addArgument<bool>("-strict")
                    .help("requires global scaling factor to be 1")
                    .action(storeTrue);
            },
            [](ArgumentParser const& parser) {
                auto ids = parser.get<vector<size_t>>("ids");
                auto eps = parser.get<double>("-epsilon");
                auto strict = parser.get<bool>("-strict");

                QTensor<double>* tensor1;
                QTensor<double>* tensor2;
                if (ids.size() == 2) {
                    tensor1 = tensorMgr.findByID(ids[0]);
                    tensor2 = tensorMgr.findByID(ids[1]);
                } else {
                    tensor1 = tensorMgr.get();
                    tensor2 = tensorMgr.findByID(ids[0]);
                }

                bool equiv = isEquivalent(*tensor1, *tensor2, eps);
                double norm = globalNorm<double>(*tensor1, *tensor2);
                Phase phase = globalPhase<double>(*tensor1, *tensor2);

                if (strict) {
                    if (norm > 1 + eps || norm < 1 - eps || phase != Phase(0)) {
                        equiv = false;
                    }
                }
                using namespace dvlab;
                if (equiv) {
                    fmt::println("{}", fmt_ext::styled_if_ANSI_supported("Equivalent", fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
                    fmt::println("- Global Norm : {:.6}", norm);
                    fmt::println("- Global Phase: {}", phase);
                } else {
                    fmt::println("{}", fmt_ext::styled_if_ANSI_supported("Not Equivalent", fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
                }

                return CmdExecResult::DONE;
            }};
}
