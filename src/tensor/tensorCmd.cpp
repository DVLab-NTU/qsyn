/****************************************************************************
  FileName     [ tensorCmd.cpp ]
  PackageName  [ tensor ]
  Synopsis     [ Define tensor package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <cstddef>  // for size_t
#include <iomanip>
#include <iostream>
#include <string>

#include "apCmd.h"
#include "cmdParser.h"
#include "phase.h"
#include "tensorMgr.h"
#include "textFormat.h"

using namespace std;
namespace TF = TextFormat;

extern TensorMgr* tensorMgr;
extern size_t verbose;

using namespace ArgParse;

unique_ptr<ArgParseCmdType> tsResetCmd();
unique_ptr<ArgParseCmdType> tsPrintCmd();
unique_ptr<ArgParseCmdType> tsAdjointCmd();
unique_ptr<ArgParseCmdType> tsEquivCmd();

bool initTensorCmd() {
    tensorMgr = new TensorMgr{};
    if (!(
            cmdMgr->regCmd("TSReset", 3, tsResetCmd()) &&
            cmdMgr->regCmd("TSPrint", 3, tsPrintCmd()) &&
            cmdMgr->regCmd("TSADJoint", 5, tsAdjointCmd()) &&
            cmdMgr->regCmd("TSEQuiv", 4, tsEquivCmd()))) {
        cerr << "Registering \"tensor\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

ArgType<size_t>::ConstraintType validTensorId = {
    [](ArgType<size_t> const& arg) {
        return [&arg]() {
            return tensorMgr->hasId(arg.getValue());
        };
    },
    [](ArgType<size_t> const& arg) {
        return [&arg]() {
            cerr << "Error: Can't find tensor with ID " << arg.getValue() << "!!" << endl;
        };
    }};

unique_ptr<ArgParseCmdType> tsResetCmd() {
    unique_ptr<ArgParseCmdType> cmd = make_unique<ArgParseCmdType>("TSReset");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("reset the tensor manager");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        tensorMgr->reset();
        return CMD_EXEC_DONE;
    };

    return cmd;
}

unique_ptr<ArgParseCmdType> tsPrintCmd() {
    unique_ptr<ArgParseCmdType> cmd = make_unique<ArgParseCmdType>("TSPrint");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("print info of stored tensors");

        parser.addArgument<bool>("-list")
            .action(storeTrue)
            .help("only list summary");
        parser.addArgument<size_t>("id")
            .required(false)
            .constraint(validTensorId)
            .help("the ID to the tensor");
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        bool list = parser["-list"];
        if (parser["id"].isParsed()) {
            tensorMgr->printTensor(parser["id"], list);
        } else {
            tensorMgr->printTensorMgr();
        }

        return CMD_EXEC_DONE;
    };

    return cmd;
}
unique_ptr<ArgParseCmdType> tsAdjointCmd() {
    unique_ptr<ArgParseCmdType> cmd = make_unique<ArgParseCmdType>("TSADJoint");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("adjoint the specified tensor");

        parser.addArgument<size_t>("id")
            .constraint(validTensorId)
            .help("the ID of the tensor");
    };
    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        tensorMgr->adjoint(parser["id"]);
        return CMD_EXEC_DONE;
    };

    return cmd;
}
unique_ptr<ArgParseCmdType> tsEquivCmd() {
    unique_ptr<ArgParseCmdType> cmd = make_unique<ArgParseCmdType>("TSEQuiv");

    cmd->parserDefinition = [](ArgumentParser& parser) {
        parser.help("check the equivalency of two stored tensors");

        parser.addArgument<size_t>("id1")
            .help("the ID of the first tensor")
            .constraint(validTensorId);
        parser.addArgument<size_t>("id2")
            .help("the ID of the second tensor")
            .constraint(validTensorId);
        parser.addArgument<double>("-epsilon")
            .metavar("eps")
            .defaultValue(1e-6)
            .help("output \"equivalent\" if the Frobenius inner product is at least than 1 - eps (default: 1e-6)");
        parser.addArgument<bool>("-strict")
            .help("requires global scaling factor to be 1")
            .action(storeTrue);
    };

    cmd->onParseSuccess = [](ArgumentParser const& parser) {
        size_t id1 = parser["id1"], id2 = parser["id2"];
        double eps = parser["-epsilon"];
        bool strict = parser["-strict"];

        bool equiv = tensorMgr->isEquivalent(id1, id2, eps);
        double norm = tensorMgr->getGlobalNorm(id1, id2);
        Phase phase = tensorMgr->getGlobalPhase(id1, id2);

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

        return CMD_EXEC_DONE;
    };

    return cmd;
}
