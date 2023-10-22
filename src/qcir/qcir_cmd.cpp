/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define qcir package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir/qcir_cmd.hpp"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <filesystem>
#include <string>

#include "./qcir_gate.hpp"
#include "cli/cli.hpp"
#include "qcir/qcir.hpp"
#include "tensor/tensor_mgr.hpp"
#include "util/phase.hpp"
#include "zx/zxgraph_mgr.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::qcir {

bool qcir_mgr_not_empty(QCirMgr const& qcir_mgr) {
    if (qcir_mgr.empty()) {
        spdlog::error("QCir list is empty. Please create a QCir first!!");
        spdlog::info("Use QCNew/QCBAdd to add a new QCir, or QCCRead to read a QCir from a file.");
        return false;
    }
    return true;
}

std::function<bool(size_t const&)> valid_qcir_id(QCirMgr const& qcir_mgr) {
    return [&](size_t const& id) {
        if (qcir_mgr.is_id(id)) return true;
        std::cerr << "Error: QCir " << id << " does not exist!!\n";
        return false;
    };
}

std::function<bool(size_t const&)> valid_qcir_gate_id(QCirMgr const& qcir_mgr) {
    return [&](size_t const& id) {
        if (!qcir_mgr_not_empty(qcir_mgr)) return false;
        if (qcir_mgr.get()->get_gate(id) != nullptr) return true;
        std::cerr << "Error: gate id " << id << " does not exist!!\n";
        return false;
    };
}

std::function<bool(QubitIdType const&)> valid_qcir_qubit_id(QCirMgr const& qcir_mgr) {
    return [&](QubitIdType const& id) {
        if (!qcir_mgr_not_empty(qcir_mgr)) return false;
        if (qcir_mgr.get()->get_qubit(id) != nullptr) return true;
        std::cerr << "Error: Qubit ID " << id << " does not exist!!\n";
        return false;
    };
}

//----------------------------------------------------------------------
//    QCCHeckout <(size_t id)>
//----------------------------------------------------------------------

dvlab::Command qcir_checkout_cmd(QCirMgr& qcir_mgr) {
    return {"qccheckout",
            [&](ArgumentParser& parser) {
                parser.description("checkout to QCir <id> in QCirMgr");

                parser.add_argument<size_t>("id")
                    .constraint(valid_qcir_id(qcir_mgr))
                    .help("the ID of the circuit");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                qcir_mgr.checkout(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCReset
//----------------------------------------------------------------------

dvlab::Command qcir_mgr_reset_cmd(QCirMgr& qcir_mgr) {
    return {"qcreset",
            [](ArgumentParser& parser) {
                parser.description("reset QCirMgr");
            },
            [&](ArgumentParser const&) {
                qcir_mgr.clear();
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCDelete <(size_t id)>
//----------------------------------------------------------------------

dvlab::Command qcir_delete_cmd(QCirMgr& qcir_mgr) {
    return {"qcdelete",
            [&](ArgumentParser& parser) {
                parser.description("remove a QCir from QCirMgr");

                parser.add_argument<size_t>("id")
                    .constraint(valid_qcir_id(qcir_mgr))
                    .help("the ID of the circuit");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                qcir_mgr.remove(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCNew [(size_t id)]
//----------------------------------------------------------------------

dvlab::Command qcir_new_cmd(QCirMgr& qcir_mgr) {
    return {"qcnew",
            [](ArgumentParser& parser) {
                parser.description("create a new QCir to QCirMgr");

                parser.add_argument<size_t>("id")
                    .nargs(NArgsOption::optional)
                    .help("the ID of the circuit");

                parser.add_argument<bool>("-replace")
                    .action(store_true)
                    .help("if specified, replace the current circuit; otherwise store to a new one");
            },
            [&](ArgumentParser const& parser) {
                auto const id = parser.parsed("id") ? parser.get<size_t>("id") : qcir_mgr.get_next_id();

                if (qcir_mgr.is_id(id)) {
                    if (!parser.parsed("-Replace")) {
                        std::cerr << "Error: QCir " << id << " already exists!! Specify `-Replace` if needed.\n";
                        return CmdExecResult::error;
                    }

                    qcir_mgr.set(std::make_unique<QCir>());
                } else {
                    qcir_mgr.add(id);
                }

                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCCOPy [size_t id] [-Replace]
//----------------------------------------------------------------------

dvlab::Command qcir_copy_cmd(QCirMgr& qcir_mgr) {
    return {"qccopy",
            [](ArgumentParser& parser) {
                parser.description("copy a QCir to QCirMgr");

                parser.add_argument<size_t>("id")
                    .nargs(NArgsOption::optional)
                    .help("the ID copied circuit to be stored");

                parser.add_argument<bool>("-replace")
                    .default_value(false)
                    .action(store_true)
                    .help("replace the current focused circuit");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                size_t const id = parser.parsed("id") ? parser.get<size_t>("id") : qcir_mgr.get_next_id();
                if (qcir_mgr.is_id(id) && !parser.parsed("-replace")) {
                    std::cerr << "Error: QCir " << id << " already exists!! Specify `-Replace` if needed." << std::endl;
                    return CmdExecResult::error;
                }
                qcir_mgr.copy(id);

                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCCOMpose <size_t id>
//----------------------------------------------------------------------

dvlab::Command qcir_compose_cmd(QCirMgr& qcir_mgr) {
    return {"qccompose",
            [&](ArgumentParser& parser) {
                parser.description("compose a QCir");

                parser.add_argument<size_t>("id")
                    .constraint(valid_qcir_id(qcir_mgr))
                    .help("the ID of the circuit to compose with");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                qcir_mgr.get()->compose(*qcir_mgr.find_by_id(parser.get<size_t>("id")));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCTensor <size_t id>
//----------------------------------------------------------------------

dvlab::Command qcir_tensor_product_cmd(QCirMgr& qcir_mgr) {
    return {"qctensor",
            [&](ArgumentParser& parser) {
                parser.description("tensor a QCir");

                parser.add_argument<size_t>("id")
                    .constraint(valid_qcir_id(qcir_mgr))
                    .help("the ID of the circuit to tensor with");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                qcir_mgr.get()->tensor_product(*qcir_mgr.find_by_id(parser.get<size_t>("id")));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCPrint [-SUmmary | -Focus | -Num | -SEttings]
//----------------------------------------------------------------------

dvlab::Command qcir_mgr_print_cmd(QCirMgr const& qcir_mgr) {
    return {"qcprint",
            [](ArgumentParser& parser) {
                parser.description("print info about QCirs or settings");

                auto mutex = parser.add_mutually_exclusive_group();

                mutex.add_argument<bool>("-focus")
                    .action(store_true)
                    .help("print the info of circuit in focus");
                mutex.add_argument<bool>("-list")
                    .action(store_true)
                    .help("print a list of circuits");
                mutex.add_argument<bool>("-settings")
                    .action(store_true)
                    .help("print settings of circuit");
            },
            [&](ArgumentParser const& parser) {
                if (parser.parsed("-settings")) {
                    std::cout << std::endl;
                    std::cout << "Delay of Single-qubit gate :     " << SINGLE_DELAY << std::endl;
                    std::cout << "Delay of Double-qubit gate :     " << DOUBLE_DELAY << std::endl;
                    std::cout << "Delay of SWAP gate :             " << SWAP_DELAY << ((SWAP_DELAY == 3 * DOUBLE_DELAY) ? " (3 CXs)" : "") << std::endl;
                    std::cout << "Delay of Multiple-qubit gate :   " << MULTIPLE_DELAY << std::endl;
                } else if (parser.parsed("-focus"))
                    qcir_mgr.print_focus();
                else if (parser.parsed("-list"))
                    qcir_mgr.print_list();
                else
                    qcir_mgr.print_manager();

                return CmdExecResult::done;
            }};
}

//------------------------------------------------------------------------------
//    QCSet ...
//------------------------------------------------------------------------------

dvlab::Command qcir_settings_cmd() {
    return {"qcset",
            [](ArgumentParser& parser) {
                parser.description("set QCir parameters");

                parser.add_argument<size_t>("-single-delay")
                    .help("delay of single-qubit gate");
                parser.add_argument<size_t>("-double-delay")
                    .help("delay of double-qubit gate, SWAP excluded");
                parser.add_argument<size_t>("-swap-delay")
                    .help("delay of SWAP gate, used to be 3x double-qubit gate");
                parser.add_argument<size_t>("-multiple-delay")
                    .help("delay of multiple-qubit gate");
            },
            [](ArgumentParser const& parser) {
                if (parser.parsed("-single-delay")) {
                    auto single_delay = parser.get<size_t>("-single-delay");
                    if (single_delay == 0)
                        std::cerr << "Error: single delay value should > 0, skipping this option!!\n";
                    else
                        SINGLE_DELAY = single_delay;
                }
                if (parser.parsed("-double-delay")) {
                    auto double_delay = parser.get<size_t>("-double-delay");
                    if (double_delay == 0)
                        std::cerr << "Error: double delay value should > 0, skipping this option!!\n";
                    else
                        DOUBLE_DELAY = double_delay;
                }
                if (parser.parsed("-swap-delay")) {
                    auto swap_delay = parser.get<size_t>("-swap-delay");
                    if (swap_delay == 0)
                        std::cerr << "Error: swap delay value should > 0, skipping this option!!\n";
                    else
                        SWAP_DELAY = swap_delay;
                }
                if (parser.parsed("-multiple-delay")) {
                    auto multi_delay = parser.get<size_t>("-multiple-delay");
                    if (multi_delay == 0)
                        std::cerr << "Error: multiple delay value should > 0, skipping this option!!\n";
                    else
                        MULTIPLE_DELAY = multi_delay;
                }
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCCRead <(string fileName)> [-Replace]
//----------------------------------------------------------------------

dvlab::Command qcir_read_cmd(QCirMgr& qcir_mgr) {
    return {"qccread",
            [](ArgumentParser& parser) {
                parser.description("read a circuit and construct the corresponding netlist");

                parser.add_argument<std::string>("filepath")
                    .constraint(path_readable)
                    .constraint(allowed_extension({".qasm", ".qc", ".qsim", ".quipper", ""}))
                    .help("the filepath to quantum circuit file. Supported extension: .qasm, .qc, .qsim, .quipper");

                parser.add_argument<bool>("-replace")
                    .action(store_true)
                    .help("if specified, replace the current circuit; otherwise store to a new one");
            },
            [&](ArgumentParser const& parser) {
                QCir buffer_q_cir;
                auto filepath = parser.get<std::string>("filepath");
                auto replace  = parser.get<bool>("-replace");
                if (!buffer_q_cir.read_qcir_file(filepath)) {
                    std::cerr << "Error: the format in \"" << filepath << "\" has something wrong!!" << std::endl;
                    return CmdExecResult::error;
                }
                if (qcir_mgr.empty() || !replace) {
                    qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<QCir>(std::move(buffer_q_cir)));
                } else {
                    qcir_mgr.set(std::make_unique<QCir>(std::move(buffer_q_cir)));
                }
                qcir_mgr.get()->set_filename(std::filesystem::path{filepath}.stem());
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCGPrint <(size_t gateID)> [-Time | -ZXform]
//----------------------------------------------------------------------

dvlab::Command qcir_gate_print_cmd(QCirMgr const& qcir_mgr) {
    return {"qcgprint",
            [&](ArgumentParser& parser) {
                parser.description("print gate info in QCir");

                parser.add_argument<size_t>("id")
                    .constraint(valid_qcir_gate_id(qcir_mgr))
                    .help("the id of the gate");

                parser.add_argument<bool>("-time")
                    .action(store_true)
                    .help("print the execution time of the gate");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                qcir_mgr.get()->print_gate_info(parser.get<size_t>("id"), parser.parsed("-time"));

                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCCPrint [-Summary | -Analysis | -Detail | -List | -Qubit]
//----------------------------------------------------------------------

dvlab::Command qcir_print_cmd(QCirMgr const& qcir_mgr) {
    return {"qccprint",
            [](ArgumentParser& parser) {
                parser.description("print info of QCir");

                auto mutex = parser.add_mutually_exclusive_group();

                mutex.add_argument<bool>("-summary")
                    .action(store_true)
                    .help("print summary of the circuit");
                mutex.add_argument<bool>("-analysis")
                    .action(store_true)
                    .help("virtually decompose the circuit and print information");
                mutex.add_argument<bool>("-detail")
                    .action(store_true)
                    .help("print the constitution of the circuit");
                mutex.add_argument<bool>("-list")
                    .action(store_true)
                    .help("print a list of gates in the circuit");
                mutex.add_argument<bool>("-qubit")
                    .action(store_true)
                    .help("print the circuit along the qubits");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                if (parser.parsed("-analysis"))
                    qcir_mgr.get()->count_gates(false);
                else if (parser.parsed("-detail"))
                    qcir_mgr.get()->count_gates(true);
                else if (parser.parsed("-list"))
                    qcir_mgr.get()->print_gates();
                else if (parser.parsed("-qubit"))
                    qcir_mgr.get()->print_qubits();
                else if (parser.parsed("-summary"))
                    qcir_mgr.get()->print_summary();
                else
                    qcir_mgr.get()->print_qcir_info();

                return CmdExecResult::done;
            }};
}

//-----------------------------------------------------------------------------------------------------------------------------------------------
//     QCGAdd <-H | -X | -Z | -T | -TDG | -S | -SX> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-CX | -CZ> <(size_t ctrl)> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-CCX | -CCZ> <(size_t ctrl1)> <(size_t ctrl2)> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-P | -PX | -RZ | -RX> <-PHase (Phase phase_inp)> <(size_t targ)> [-APpend|-PRepend] /
//     QCGAdd <-MCP | -MCPX | -MCRZ| -MCRX> <-PHase (Phase phase_inp)> <(size_t ctrl1)> ... <(size_t ctrln)> <(size_t targ)> [-APpend|-PRepend]
//-----------------------------------------------------------------------------------------------------------------------------------------------

dvlab::Command qcir_gate_add_cmd(QCirMgr& qcir_mgr) {
    static dvlab::utils::ordered_hashmap<std::string, std::string> single_qubit_gates_no_phase = {
        {"h", "Hadamard gate"},
        {"x", "Pauli-X gate"},
        {"y", "Pauli-Y gate"},
        {"z", "Pauli-Z gate"},
        {"t", "T gate"},
        {"tdg", "T† gate"},
        {"s", "S gate"},
        {"sdg", "S† gate"},
        {"sx", "√X gate"},
        {"sy", "√Y gate"}};

    static dvlab::utils::ordered_hashmap<std::string, std::string> single_qubit_gates_with_phase = {
        {"rz", "Rz(θ) gate"},
        {"ry", "Rx(θ) gate"},
        {"rx", "Ry(θ) gate"},
        {"p", "P = (e^iθ/2)Rz gate"},
        {"pz", "Pz = (e^iθ/2)Rz gate"},
        {"px", "Px = (e^iθ/2)Rx gate"},
        {"py", "Py = (e^iθ/2)Ry gate"}  //
    };

    static dvlab::utils::ordered_hashmap<std::string, std::string> double_qubit_gates_no_phase = {
        {"cx", "CX (CNOT) gate"},
        {"cz", "CZ gate"},
        {"swap", "SWAP gate"}};

    static dvlab::utils::ordered_hashmap<std::string, std::string> three_qubit_gates_no_phase = {
        {"ccx", "CCX (CCNOT, Toffoli) gate"},
        {"ccz", "CCZ gate"}  //
    };

    static dvlab::utils::ordered_hashmap<std::string, std::string> multi_qubit_gates_with_phase = {
        {"mcrz", "Multi-Controlled Rz(θ) gate"},
        {"mcrx", "Multi-Controlled Rx(θ) gate"},
        {"mcry", "Multi-Controlled Ry(θ) gate"},
        {"mcp", "Multi-Controlled P(θ) gate"},
        {"mcpz", "Multi-Controlled Pz(θ) gate"},
        {"mcpx", "Multi-Controlled Px(θ) gate"},
        {"mcpy", "Multi-Controlled Py(θ) gate"}  //
    };

    return {
        "qcgadd",
        [=, &qcir_mgr](ArgumentParser& parser) {
            parser.description("add quantum gate");

            std::vector<std::string> type_choices;
            std::string type_help =
                "the quantum gate type.\n"
                "For control gates, the control qubits comes before the target qubits.";

            for (auto& category : {
                     single_qubit_gates_no_phase,
                     single_qubit_gates_with_phase,
                     double_qubit_gates_no_phase,
                     three_qubit_gates_no_phase,
                     multi_qubit_gates_with_phase}) {
                for (auto& [name, help] : category) {
                    type_choices.emplace_back(name);
                    type_help += '\n' + name + ": ";
                    if (name.size() < 4) type_help += std::string(4 - name.size(), ' ');
                    type_help += help;
                }
            }
            parser.add_argument<std::string>("type")
                .help(type_help)
                .constraint(choices_allow_prefix(type_choices));

            auto append_or_prepend = parser.add_mutually_exclusive_group().required(false);
            append_or_prepend.add_argument<bool>("-append")
                .help("append the gate at the end of QCir")
                .action(store_true);
            append_or_prepend.add_argument<bool>("-prepend")
                .help("prepend the gate at the start of QCir")
                .action(store_true);

            parser.add_argument<dvlab::Phase>("-phase")
                .help("The rotation angle θ (default = π). This option must be specified if and only if the gate type takes a phase parameter.");

            parser.add_argument<QubitIdType>("qubits")
                .nargs(NArgsOption::zero_or_more)
                .constraint(valid_qcir_qubit_id(qcir_mgr))
                .help("the qubits on which the gate applies");
        },
        [=, &qcir_mgr](ArgumentParser const& parser) {
            if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
            bool const do_prepend = parser.parsed("-prepend");

            auto type = parser.get<std::string>("type");
            type      = dvlab::str::tolower_string(type);

            auto is_gate_category = [&](auto& category) {
                return any_of(category.begin(), category.end(),
                              [&](auto& name_help) {
                                  return type == name_help.first;
                              });
            };

            dvlab::Phase phase{1};
            if (is_gate_category(single_qubit_gates_with_phase) ||
                is_gate_category(multi_qubit_gates_with_phase)) {
                if (!parser.parsed("-phase")) {
                    std::cerr << "Error: phase must be specified for gate type " << type << "!!\n";
                    return CmdExecResult::error;
                }
                phase = parser.get<dvlab::Phase>("-phase");
            } else if (parser.parsed("-phase")) {
                std::cerr << "Error: phase is incompatible with gate type " << type << "!!\n";
                return CmdExecResult::error;
            }

            auto bits = parser.get<QubitIdList>("qubits");

            if (is_gate_category(single_qubit_gates_no_phase) ||
                is_gate_category(single_qubit_gates_with_phase)) {
                if (bits.size() < 1) {
                    std::cerr << "Error: too few qubits are supplied for gate " << type << "!!\n";
                    return CmdExecResult::error;
                } else if (bits.size() > 1) {
                    std::cerr << "Error: too many qubits are supplied for gate " << type << "!!\n";
                    return CmdExecResult::error;
                }
            }

            if (is_gate_category(double_qubit_gates_no_phase)) {
                if (bits.size() < 2) {
                    std::cerr << "Error: too few qubits are supplied for gate " << type << "!!\n";
                    return CmdExecResult::error;
                } else if (bits.size() > 2) {
                    std::cerr << "Error: too many qubits are supplied for gate " << type << "!!\n";
                    return CmdExecResult::error;
                }
            }

            if (is_gate_category(three_qubit_gates_no_phase)) {
                if (bits.size() < 3) {
                    std::cerr << "Error: too few qubits are supplied for gate " << type << "!!\n";
                    return CmdExecResult::error;
                } else if (bits.size() > 3) {
                    std::cerr << "Error: too many qubits are supplied for gate " << type << "!!\n";
                    return CmdExecResult::error;
                }
            }

            qcir_mgr.get()->add_gate(type, bits, phase, !do_prepend);

            return CmdExecResult::done;
        }};
}

//----------------------------------------------------------------------
//    QCBAdd [size_t addNum]
//----------------------------------------------------------------------

dvlab::Command qcir_qubit_add_cmd(QCirMgr& qcir_mgr) {
    return {"qcbadd",
            [](ArgumentParser& parser) {
                parser.description("add qubit(s)");

                parser.add_argument<size_t>("amount")
                    .nargs(NArgsOption::optional)
                    .help("the amount of qubits to be added");
            },
            [&](ArgumentParser const& parser) {
                if (qcir_mgr.empty()) {
                    std::cout << "Note: QCir list is empty now. Create a new one." << std::endl;
                    qcir_mgr.add(qcir_mgr.get_next_id());
                }
                if (parser.parsed("amount"))
                    qcir_mgr.get()->add_qubits(parser.get<size_t>("amount"));
                else
                    qcir_mgr.get()->add_qubits(1);
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCGDelete <(size_t gateID)>
//----------------------------------------------------------------------

dvlab::Command qcir_gate_delete_cmd(QCirMgr& qcir_mgr) {
    return {"qcgdelete",
            [&](ArgumentParser& parser) {
                parser.description("delete gate");

                parser.add_argument<size_t>("id")
                    .constraint(valid_qcir_gate_id(qcir_mgr))
                    .help("the id to be deleted");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                qcir_mgr.get()->remove_gate(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCBDelete <(size_t qubitID)>
//----------------------------------------------------------------------

dvlab::Command qcir_qubit_delete_cmd(QCirMgr& qcir_mgr) {
    return {"qcbdelete",
            [&](ArgumentParser& parser) {
                parser.description("delete qubit");

                parser.add_argument<QubitIdType>("id")
                    .constraint(valid_qcir_qubit_id(qcir_mgr))
                    .help("the id to be deleted");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                if (!qcir_mgr.get()->remove_qubit(parser.get<QubitIdType>("id")))
                    return CmdExecResult::error;
                else
                    return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    QCCWrite
//----------------------------------------------------------------------

dvlab::Command qcir_write_cmd(QCirMgr const& qcir_mgr) {
    return {"qccwrite",
            [](ArgumentParser& parser) {
                parser.description("write QCir to a QASM file");

                parser.add_argument<std::string>("output_path")
                    .constraint(path_writable)
                    .constraint(allowed_extension({".qasm"}))
                    .help("the filepath to output file. Supported extension: .qasm");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                if (!qcir_mgr.get()->write_qasm(parser.get<std::string>("output_path"))) {
                    std::cerr << "Error: path " << parser.get<std::string>("output_path") << " not found!!" << std::endl;
                    return CmdExecResult::error;
                }
                return CmdExecResult::done;
            }};
}

Command qcir_draw_cmd(QCirMgr const& qcir_mgr) {
    return {"qccdraw",
            [](ArgumentParser& parser) {
                parser.description("draw a QCir. This command relies on qiskit and pdflatex to be present in the system");

                parser.add_argument<std::string>("output_path")
                    .nargs(NArgsOption::optional)
                    .constraint(path_writable)
                    .default_value("")
                    .help(
                        "if specified, output the resulting drawing into this file. "
                        "This argument is mandatory if the drawer is 'mpl' or 'latex'");
                parser.add_argument<std::string>("-drawer")
                    .choices(std::initializer_list<std::string>{"text", "mpl", "latex", "latex_source"})
                    .default_value("text")
                    .help("the backend for drawing quantum circuit");
                parser.add_argument<float>("-scale")
                    .default_value(1.0f)
                    .help("if specified, scale the resulting drawing by this factor");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                auto drawer      = str_to_qcir_drawer_type(parser.get<std::string>("-drawer"));
                auto output_path = parser.get<std::string>("output_path");
                auto scale       = parser.get<float>("-scale");

                if (!drawer.has_value()) {
                    spdlog::critical("Invalid drawer type: {}", parser.get<std::string>("-drawer"));
                    spdlog::critical("This error should have been unreachable. Please report this bug to the developer.");
                    return CmdExecResult::error;
                }

                if (drawer.value() == QCirDrawerType::latex || drawer.value() == QCirDrawerType::latex_source) {
                    if (output_path.empty()) {
                        std::cerr << "Error: Using drawer \"" << fmt::format("{}", drawer.value()) << "\" requires an output destination!!" << std::endl;
                        return CmdExecResult::error;
                    }
                }

                if (drawer == QCirDrawerType::text && parser.parsed("-scale")) {
                    std::cerr << "Error: Cannot set scale for \'text\' drawer!!" << std::endl;
                    return CmdExecResult::error;
                }

                if (!qcir_mgr.get()->draw(drawer.value(), output_path, scale)) {
                    std::cerr << "Error: could not draw the QCir successfully!!" << std::endl;
                    return CmdExecResult::error;
                }

                return CmdExecResult::done;
            }};
}

bool add_qcir_cmds(dvlab::CommandLineInterface& cli, QCirMgr& qcir_mgr) {
    if (!(cli.add_command(qcir_checkout_cmd(qcir_mgr)) &&
          cli.add_command(qcir_mgr_reset_cmd(qcir_mgr)) &&
          cli.add_command(qcir_delete_cmd(qcir_mgr)) &&
          cli.add_command(qcir_new_cmd(qcir_mgr)) &&
          cli.add_command(qcir_copy_cmd(qcir_mgr)) &&
          cli.add_command(qcir_compose_cmd(qcir_mgr)) &&
          cli.add_command(qcir_tensor_product_cmd(qcir_mgr)) &&
          cli.add_command(qcir_mgr_print_cmd(qcir_mgr)) &&
          cli.add_command(qcir_settings_cmd()) &&
          cli.add_command(qcir_read_cmd(qcir_mgr)) &&
          cli.add_command(qcir_print_cmd(qcir_mgr)) &&
          cli.add_command(qcir_gate_add_cmd(qcir_mgr)) &&
          cli.add_command(qcir_qubit_add_cmd(qcir_mgr)) &&
          cli.add_command(qcir_gate_delete_cmd(qcir_mgr)) &&
          cli.add_command(qcir_qubit_delete_cmd(qcir_mgr)) &&
          cli.add_command(qcir_gate_print_cmd(qcir_mgr)) &&
          cli.add_command(qcir_draw_cmd(qcir_mgr)) &&
          cli.add_command(qcir_write_cmd(qcir_mgr)))) {
        spdlog::error("Registering \"qcir\" commands fails... exiting");
        return false;
    }
    return true;
}

}  // namespace qsyn::qcir
