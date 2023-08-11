/****************************************************************************
  FileName     [ tensorCmd.cpp ]
  PackageName  [ tensor ]
  Synopsis     [ Define tensor package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>

#include "./qtensor.hpp"
#include "./tensorMgr.hpp"
#include "cli/cli.hpp"
#include "util/phase.hpp"
#include "util/textFormat.hpp"

using namespace std;
namespace TF = TextFormat;

TensorMgr tensorMgr{"Tensor"};
extern size_t verbose;

using namespace ArgParse;

unique_ptr<ArgParseCmdType> TensorMgrResetCmd();
unique_ptr<ArgParseCmdType> TensorMgrPrintCmd();
unique_ptr<ArgParseCmdType> TensorAdjointCmd();
unique_ptr<ArgParseCmdType> TensorPrintCmd();
unique_ptr<ArgParseCmdType> TensorEquivalenceCmd();

bool initTensorCmd() {
    if (!(
            cli.regCmd("TSReset", 3, TensorMgrResetCmd()) &&
            cli.regCmd("TSPrint", 3, TensorMgrPrintCmd()) &&
            cli.regCmd("TSADJoint", 5, TensorAdjointCmd()) &&
            cli.regCmd("TSTPrint", 4, TensorPrintCmd()) &&
            cli.regCmd("TSEQuiv", 4, TensorEquivalenceCmd()))) {
        cerr << "Registering \"tensor\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType validTensorId = {
    [](size_t const& id) {
        return tensorMgr.isID(id);
    },
    [](size_t const& id) {
        cerr << "Error: Can't find tensor with ID " << id << "!!" << endl;
    }};

unique_ptr<ArgParseCmdType> TensorMgrResetCmd() {
    unique_ptr<ArgParseCmdType> cmd = make_unique<ArgParseCmdType>("TSReset");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("reset the tensor manager");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        tensorMgr.reset();
        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> TensorMgrPrintCmd() {
    unique_ptr<ArgParseCmdType> cmd = make_unique<ArgParseCmdType>("TSPrint");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("print info of TensorMgr");
        auto mutex = parser.addMutuallyExclusiveGroup().required(false);

        mutex.addArgument<bool>("-summary")
            .action(storeTrue)
            .help("print summary of all Tensors");
        mutex.addArgument<bool>("-focus")
            .action(storeTrue)
            .help("print the info of the Tensor in focus");
        mutex.addArgument<bool>("-list")
            .action(storeTrue)
            .help("print a list of Tensors");
        mutex.addArgument<bool>("-number")
            .action(storeTrue)
            .help("print the number of Tensors managed");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser["-focus"].isParsed())
            tensorMgr.printFocus();
        else if (parser["-number"].isParsed())
            tensorMgr.printListSize();
        else if (parser["-list"].isParsed())
            tensorMgr.printList();
        else
            tensorMgr.printMgr();

        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> TensorPrintCmd() {
    unique_ptr<ArgParseCmdType> cmd = make_unique<ArgParseCmdType>("TSTPrint");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("print info of Tensor");

        parser.addArgument<size_t>("id")
            .constraint(validTensorId)
            .nargs(NArgsOption::OPTIONAL)
            .help("if specified, print the tensor with the ID");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser["id"].isParsed()) {
            cout << *tensorMgr.findByID(parser["id"]) << endl;
        } else {
            cout << *tensorMgr.get() << endl;
        }

        return CmdExecResult::DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> TensorAdjointCmd() {
    unique_ptr<ArgParseCmdType> cmd = make_unique<ArgParseCmdType>("TSADJoint");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("adjoint the specified tensor");

        parser.addArgument<size_t>("id")
            .constraint(validTensorId)
            .nargs(NArgsOption::OPTIONAL)
            .help("the ID of the tensor");
    };
    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        if (parser["id"].isParsed()) {
            tensorMgr.findByID(parser["id"])->adjoint();
        } else {
            tensorMgr.get()->adjoint();
        }
        return CmdExecResult::DONE;
    };

    return cmd;
}
unique_ptr<ArgParseCmdType> TensorEquivalenceCmd() {
    unique_ptr<ArgParseCmdType> cmd = make_unique<ArgParseCmdType>("TSEQuiv");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("check the equivalency of two stored tensors");
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
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        vector<size_t> ids = parser["ids"];
        double eps = parser["-epsilon"];
        bool strict = parser["-strict"];

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

        if (equiv) {
            cout << TF::BOLD(TF::GREEN("Equivalent")) << endl
                 << "- Global Norm : " << norm << endl
                 << "- Global Phase: " << phase << endl;
        } else {
            cout << TF::BOLD(TF::RED("Not Equivalent")) << endl;
        }

        return CmdExecResult::DONE;
    };

    return cmd;
}
