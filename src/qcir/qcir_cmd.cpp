/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define qcir package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "qcir/qcir_cmd.hpp"

#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <filesystem>
#include <ostream>
#include <string>
#include "bits/stdc++.h"

#include "./optimizer/optimizer_cmd.hpp"
#include "./qcir_gate.hpp"
#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_mgr.hpp"
#include "util/cin_cout_cerr.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/dvlab_string.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

using namespace dvlab::argparse;
using namespace std;
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
        spdlog::error("QCir {} does not exist!!", id);
        return false;
    };
}

std::function<bool(size_t const&)> valid_qcir_gate_id(QCirMgr const& qcir_mgr) {
    return [&](size_t const& id) {
        if (!qcir_mgr_not_empty(qcir_mgr)) return false;
        if (qcir_mgr.get()->get_gate(id) != nullptr) return true;
        spdlog::error("Gate ID {} does not exist!!", id);
        return false;
    };
}

std::function<bool(QubitIdType const&)> valid_qcir_qubit_id(QCirMgr const& qcir_mgr) {
    return [&](QubitIdType const& id) {
        if (!qcir_mgr_not_empty(qcir_mgr)) return false;
        if (qcir_mgr.get()->get_qubit(id) != nullptr) return true;
        spdlog::error("Qubit ID {} does not exist!!", id);
        return false;
    };
}

dvlab::Command qcir_compose_cmd(QCirMgr& qcir_mgr) {
    return {"compose",
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

dvlab::Command qcir_tensor_product_cmd(QCirMgr& qcir_mgr) {
    return {"tensor-product",
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

dvlab::Command qcir_config_cmd() {
    return {"config",
            [](ArgumentParser& parser) {
                parser.description("set QCir parameters");

                parser.add_argument<size_t>("--single-delay")
                    .help("delay of single-qubit gate");
                parser.add_argument<size_t>("--double-delay")
                    .help("delay of double-qubit gate, SWAP excluded");
                parser.add_argument<size_t>("--swap-delay")
                    .help("delay of SWAP gate, used to be 3x double-qubit gate");
                parser.add_argument<size_t>("--multiple-delay")
                    .help("delay of multiple-qubit gate");
            },
            [](ArgumentParser const& parser) {
                auto printing_config = true;
                if (parser.parsed("--single-delay")) {
                    auto single_delay = parser.get<size_t>("--single-delay");
                    if (single_delay == 0) {
                        fmt::println("Error: single delay value should > 0, skipping this option!!");
                    } else {
                        SINGLE_DELAY = single_delay;
                    }
                    printing_config = false;
                }
                if (parser.parsed("--double-delay")) {
                    auto double_delay = parser.get<size_t>("--double-delay");
                    if (double_delay == 0) {
                        fmt::println("Error: double delay value should > 0, skipping this option!!");
                    } else {
                        DOUBLE_DELAY = double_delay;
                    }
                    printing_config = false;
                }
                if (parser.parsed("--swap-delay")) {
                    auto swap_delay = parser.get<size_t>("--swap-delay");
                    if (swap_delay == 0) {
                        fmt::println("Error: swap delay value should > 0, skipping this option!!");
                    } else {
                        SWAP_DELAY = swap_delay;
                    }
                    printing_config = false;
                }
                if (parser.parsed("--multiple-delay")) {
                    auto multi_delay = parser.get<size_t>("--multiple-delay");
                    if (multi_delay == 0) {
                        fmt::println("Error: multiple delay value should > 0, skipping this option!!");
                    } else {
                        MULTIPLE_DELAY = multi_delay;
                    }
                    printing_config = false;
                }

                if (printing_config) {
                    fmt::println("");
                    fmt::println("Delay of Single-qubit gate :     {}", SINGLE_DELAY);
                    fmt::println("Delay of Double-qubit gate :     {}", DOUBLE_DELAY);
                    fmt::println("Delay of SWAP gate :             {} {}", SWAP_DELAY, (SWAP_DELAY == 3 * DOUBLE_DELAY) ? "(3 CXs)" : "");
                    fmt::println("Delay of Multiple-qubit gate :   {}", MULTIPLE_DELAY);
                }

                return CmdExecResult::done;
            }};
}

dvlab::Command qcir_read_cmd(QCirMgr& qcir_mgr) {
    return {"read",
            [](ArgumentParser& parser) {
                parser.description("read a circuit and construct the corresponding netlist");

                parser.add_argument<std::string>("filepath")
                    .constraint(path_readable)
                    .constraint(allowed_extension({".qasm", ".qc", ".qsim", ".quipper", ""}))
                    .help("the filepath to quantum circuit file. Supported extension: .qasm, .qc, .qsim, .quipper");

                parser.add_argument<bool>("-r", "--replace")
                    .action(store_true)
                    .help("if specified, replace the current circuit; otherwise store to a new one");
            },
            [&](ArgumentParser const& parser) {
                QCir buffer_q_cir;
                auto filepath = parser.get<std::string>("filepath");
                auto replace  = parser.get<bool>("--replace");
                if (!buffer_q_cir.read_qcir_file(filepath)) {
                    fmt::println("Error: the format in \"{}\" has something wrong!!", filepath);
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

dvlab::Command qcir_write_cmd(QCirMgr const& qcir_mgr) {
    return {"write",
            [](ArgumentParser& parser) {
                parser.description("write QCir to a QASM file");

                parser.add_argument<std::string>("output_path")
                    .nargs(NArgsOption::optional)
                    .constraint(path_writable)
                    .constraint(allowed_extension({".qasm"}))
                    .help("the filepath to output file. Supported extension: .qasm. If not specified, the result will be dumped to the terminal");

                parser.add_argument<std::string>("-f", "--format")
                    .constraint(choices_allow_prefix({"qasm", "latex-qcircuit"}))
                    .help("the output format of the QCir. If not specified, the default format is automatically chosen based on the output file extension");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;

                enum class OutputFormat { qasm,
                                          latex_qcircuit };
                auto output_type = std::invoke([&]() -> OutputFormat {
                    if (parser.parsed("--format")) {
                        if (dvlab::str::is_prefix_of(parser.get<std::string>("--format"), "qasm")) return OutputFormat::qasm;
                        if (dvlab::str::is_prefix_of(parser.get<std::string>("--format"), "latex-qcircuit")) return OutputFormat::latex_qcircuit;
                        // if (dvlab::str::is_prefix_of(parser.get<std::string>("--format"), "latex-yquant")) return OutputFormat::latex_yquant;
                        DVLAB_UNREACHABLE("Invalid output format!!");
                    }

                    auto extension = std::filesystem::path{parser.get<std::string>("output_path")}.extension().string();
                    if (extension == ".qasm") return OutputFormat::qasm;
                    if (extension == ".tex") return OutputFormat::latex_qcircuit;
                    return OutputFormat::qasm;
                });

                switch (output_type) {
                    case OutputFormat::qasm:
                        if (parser.parsed("output_path")) {
                            std::ofstream file{parser.get<std::string>("output_path")};
                            if (!file) {
                                spdlog::error("Path {} not found!!", parser.get<std::string>("output_path"));
                                return CmdExecResult::error;
                            }
                            fmt::print(file, "{}", to_qasm(*qcir_mgr.get()));
                        } else {
                            fmt::print("{}", to_qasm(*qcir_mgr.get()));
                        }
                        break;
                    case OutputFormat::latex_qcircuit:
                        qcir_mgr.get()->draw(QCirDrawerType::latex_source);
                        break;
                    default:
                        DVLAB_UNREACHABLE("Invalid output format!!");
                }
                return CmdExecResult::done;
            }};
}

Command qcir_draw_cmd(QCirMgr const& qcir_mgr) {
    return {"draw",
            [](ArgumentParser& parser) {
                parser.description("draw a QCir. This command relies on qiskit and pdflatex to be present in the system");

                parser.add_argument<std::string>("output-path")
                    .constraint(path_writable)
                    .help("the output destination of the drawing");
                parser.add_argument<std::string>("-d", "--drawer")
                    .choices(std::initializer_list<std::string>{"text", "mpl", "latex"})
                    .default_value("text")
                    .help("the backend for drawing quantum circuit. If not specified, the default backend is automatically chosen based on the output file extension");
                parser.add_argument<float>("-s", "--scale")
                    .default_value(1.0f)
                    .help("if specified, scale the resulting drawing by this factor");
            },
            [&](ArgumentParser const& parser) {
                namespace fs = std::filesystem;
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;

                auto output_path = fs::path{parser.get<std::string>("output-path")};
                auto scale       = parser.get<float>("--scale");
                auto drawer      = std::invoke([&]() -> QCirDrawerType {
                    if (parser.parsed("--drawer")) return *qcir::str_to_qcir_drawer_type(parser.get<std::string>("--drawer"));

                    auto extension = output_path.extension().string();
                    if (extension == ".pdf" || extension == ".png" || extension == ".jpg" || extension == ".ps" || extension == ".eps" || extension == ".svg") return QCirDrawerType::latex;
                    return QCirDrawerType::text;
                });

                if (drawer == QCirDrawerType::text && parser.parsed("--scale")) {
                    spdlog::error("Cannot set scale for \'text\' drawer!!");
                    return CmdExecResult::error;
                }

                if (!qcir_mgr.get()->draw(drawer, output_path, scale)) {
                    spdlog::error("Could not draw the QCir successfully!!");
                    return CmdExecResult::error;
                }

                return CmdExecResult::done;
            }};
}

dvlab::Command qcir_print_cmd(QCirMgr const& qcir_mgr) {
    return {"print",
            [](ArgumentParser& parser) {
                parser.description("print info of QCir");

                parser.add_argument<bool>("-v", "--verbose")
                    .action(store_true)
                    .help("display more information");

                auto mutex = parser.add_mutually_exclusive_group();

                mutex.add_argument<bool>("-s", "--statistics")
                    .action(store_true)
                    .help("print gate statistics of the circuit. When `--verbose` is also specified, print more detailed gate counts");
                mutex.add_argument<size_t>("-g", "--gate")
                    .nargs(NArgsOption::zero_or_more)
                    .help("print information for the gates with the specified IDs. If the ID is not specified, print all gates. When `--verbose` is also specified, print the gates' predecessor and successor gates");
                mutex.add_argument<bool>("-d", "--diagram")
                    .action(store_true)
                    .help("print the circuit diagram. If `--verbose` is also specified, print the circuit diagram in the qiskit style");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) {
                    return CmdExecResult::error;
                }

                if (parser.parsed("--gate")) {
                    auto gate_ids = parser.get<std::vector<size_t>>("--gate");
                    qcir_mgr.get()->print_gates(parser.parsed("--verbose"), gate_ids);
                } else if (parser.parsed("--diagram"))
                    if (parser.parsed("--verbose")) {
                        qcir_mgr.get()->draw(QCirDrawerType::text);
                    } else {
                        qcir_mgr.get()->print_circuit_diagram();
                    }
                else if (parser.parsed("--statistics")) {
                    qcir_mgr.get()->print_qcir();
                    qcir_mgr.get()->print_gate_statistics(parser.parsed("--verbose"));
                    qcir_mgr.get()->print_depth();
                } else {
                    qcir_mgr.get()->print_qcir_info();
                }

                return CmdExecResult::done;
            }};
}

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
        "add",
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
            append_or_prepend.add_argument<bool>("--append")
                .help("append the gate at the end of QCir")
                .action(store_true);
            append_or_prepend.add_argument<bool>("--prepend")
                .help("prepend the gate at the start of QCir")
                .action(store_true);

            parser.add_argument<dvlab::Phase>("-ph", "--phase")
                .help("The rotation angle θ. This option must be specified if and only if the gate type takes a phase parameter.");

            parser.add_argument<QubitIdType>("qubits")
                .nargs(NArgsOption::zero_or_more)
                .constraint(valid_qcir_qubit_id(qcir_mgr))
                .help("the qubits on which the gate applies");
        },
        [=, &qcir_mgr](ArgumentParser const& parser) {
            if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
            bool const do_prepend = parser.parsed("--prepend");

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
                if (!parser.parsed("--phase")) {
                    spdlog::error("Phase must be specified for gate type {}!!", type);
                    return CmdExecResult::error;
                }
                phase = parser.get<dvlab::Phase>("--phase");
            } else if (parser.parsed("--phase")) {
                spdlog::error("Phase is incompatible with gate type {}!!", type);
                return CmdExecResult::error;
            }

            auto bits = parser.get<QubitIdList>("qubits");

            if (is_gate_category(single_qubit_gates_no_phase) ||
                is_gate_category(single_qubit_gates_with_phase)) {
                if (bits.size() < 1) {
                    spdlog::error("Too few qubits are supplied for gate {}!!", type);
                    return CmdExecResult::error;
                } else if (bits.size() > 1) {
                    spdlog::error("Too many qubits are supplied for gate {}!!", type);
                    return CmdExecResult::error;
                }
            }

            if (is_gate_category(double_qubit_gates_no_phase)) {
                if (bits.size() < 2) {
                    spdlog::error("Too few qubits are supplied for gate {}!!", type);
                    return CmdExecResult::error;
                } else if (bits.size() > 2) {
                    spdlog::error("Too many qubits are supplied for gate {}!!", type);
                    return CmdExecResult::error;
                }
            }

            if (is_gate_category(three_qubit_gates_no_phase)) {
                if (bits.size() < 3) {
                    spdlog::error("Too few qubits are supplied for gate {}!!", type);
                    return CmdExecResult::error;
                } else if (bits.size() > 3) {
                    spdlog::error("Too many qubits are supplied for gate {}!!", type);
                    return CmdExecResult::error;
                }
            }

            qcir_mgr.get()->add_gate(type, bits, phase, !do_prepend);

            return CmdExecResult::done;
        }};
}

dvlab::Command qcir_gate_delete_cmd(QCirMgr& qcir_mgr) {
    return {"remove",
            [&](ArgumentParser& parser) {
                parser.description("remove gate");

                parser.add_argument<size_t>("id")
                    .constraint(valid_qcir_gate_id(qcir_mgr))
                    .help("the id to be removed");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                qcir_mgr.get()->remove_gate(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

dvlab::Command qcir_gate_cmd(QCirMgr& qcir_mgr) {
    auto cmd = Command{
        "gate",
        [](ArgumentParser& parser) {
            parser.description("gate commands");

            parser.add_subparsers().required(true);
        },
        [](ArgumentParser const& /*parser*/) {
            return CmdExecResult::error;
        }};

    cmd.add_subcommand(qcir_gate_add_cmd(qcir_mgr));
    cmd.add_subcommand(qcir_gate_delete_cmd(qcir_mgr));

    return cmd;
}

dvlab::Command qcir_qubit_add_cmd(QCirMgr& qcir_mgr) {
    return {"add",
            [](ArgumentParser& parser) {
                parser.description("add qubit(s)");

                parser.add_argument<size_t>("n")
                    .nargs(NArgsOption::optional)
                    .help("the number of qubits to be added");
            },
            [&](ArgumentParser const& parser) {
                if (qcir_mgr.empty()) {
                    spdlog::info("QCir list is empty now. Create a new one.");
                    qcir_mgr.add(qcir_mgr.get_next_id());
                }

                qcir_mgr.get()->add_qubits(parser.parsed("n") ? parser.get<size_t>("n") : 1);
                return CmdExecResult::done;
            }};
}

dvlab::Command qcir_qubit_delete_cmd(QCirMgr& qcir_mgr) {
    return {"remove",
            [&](ArgumentParser& parser) {
                parser.description("remove qubit");

                parser.add_argument<QubitIdType>("id")
                    .constraint(valid_qcir_qubit_id(qcir_mgr))
                    .help("the ID of the qubit to be removed");
            },
            [&](ArgumentParser const& parser) {
                if (!qcir_mgr_not_empty(qcir_mgr)) return CmdExecResult::error;
                if (!qcir_mgr.get()->remove_qubit(parser.get<QubitIdType>("id")))
                    return CmdExecResult::error;
                else
                    return CmdExecResult::done;
            }};
}

dvlab::Command qcir_qubit_cmd(QCirMgr& qcir_mgr) {
    auto cmd = Command{
        "qubit",
        [](ArgumentParser& parser) {
            parser.description("qubit commands");

            parser.add_subparsers().required(true);
        },
        [](ArgumentParser const& /*parser*/) {
            return CmdExecResult::error;
        }};

    cmd.add_subcommand(qcir_qubit_add_cmd(qcir_mgr));
    cmd.add_subcommand(qcir_qubit_delete_cmd(qcir_mgr));

    return cmd;
}

void print_matrix(const vector<vector<complex<double>>>& matrix) {
    for (size_t i = 0; i < matrix.size(); ++i) {
        for (size_t j = 0; j < matrix[i].size(); ++j) {
            cout << matrix[i][j] << " ";
        }
        cout << endl;
    }
}

bool isUnitaryMatrix(const vector<vector<complex<double>>>& matrix) {
    // 檢查矩陣乘以其共軛轉置是否為單位矩陣
    for (size_t i = 0; i < matrix.size(); ++i) {
        for (size_t j = 0; j < matrix.size(); ++j) {
            complex<double> sum = 0.0;
            for (size_t k = 0; k < matrix.size(); ++k) {
                    
                sum += matrix[i][k] * conj(matrix[j][k]);
            }

            //for debug
            //cout<<i<<" "<<j<<" : "<<sum<<endl;
            //cout.flush();

            if (i == j && abs(sum - 1.0) > 1e-6) {
                return false;
            } else if (i != j && abs(sum) > 1e-6) {
                return false;
            }
        }
    }
    return true;
}

void conjugateMatrix(std::vector<std::vector<std::complex<double>>>& matrix) {
    for (size_t i = 0; i < matrix.size(); i++) {
        for (size_t j = 0; j < matrix[0].size(); j++) {
            matrix[i][j] = conj(matrix[i][j]); // 對每个元素共軛
        }
    }
}

vector<vector<complex<double>>> transposeMatrix(std::vector<std::vector<std::complex<double>>>& matrix) {
    vector<vector<complex<double>>> result(matrix[0].size(), vector<complex<double>>(matrix.size(), 0.0));

    for (size_t i = 0; i < matrix[0].size(); i++) {
        for (size_t j = 0; j < matrix.size(); j++) {
            result[i][j] = matrix[j][i]; // 對每個元素轉置
        }
    }
    return result;
}

vector<vector<complex<double>>> matrixMultiply(const vector<vector<complex<double>>>& a, const vector<vector<complex<double>>>& b) {
    int m = a.size();    // a 的行
    int n = a[0].size(); // a 的列
    int p = b[0].size(); // b 的列

    // init to 0
    vector<vector<complex<double>>> result(m, std::vector<complex<double>>(p, 0.0));

    // multiply
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < p; ++j) {
            for (int k = 0; k < n; ++k) {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }

    return result;
}

string intToBinary(int num, int n) {
    string binary = "";
    for (int i = n; i >= 0; --i) {
        binary += (num & (1 << i)) ? '1' : '0';
    }
    return binary;
}

double get_angle(complex<double> s) {
    return (s.real() != 0) ? atan(s.imag()/s.real()) : M_PI/2;
}

complex<double> get_det(vector<vector<complex<double>>>& U) {
    return U[0][0]*U[1][1] - U[0][1]*U[1][0];
}

vector<double> to_bloch(vector<vector<complex<double>>>& U) {
    assert(U.size() == 2);
    assert(U[0].size() == 2);
    assert(U[1].size() == 2);
    
    double theta, lambda, mu, global_phase;
    // print_matrix(U);

    theta = acos(abs(U[0][0]));
    global_phase = get_angle(get_det(U)) / 2;
    lambda = get_angle(U[0][0]) - global_phase;
    mu = get_angle(U[0][1]) - global_phase;
    // cout << theta << " " << lambda << " " << mu << " " << global_phase << endl;
    // if (global_phase > 1e-6) cerr << "not su" << endl;

    vector<double> bloch{theta, lambda, mu};
    if (abs(pow(abs(U[0][0]),2) + pow(abs(U[0][1]),2) - 1) > 1e-6) {
        cerr << "||U|| != 1" << endl;
        bloch.clear(); // ||U|| != 1
    }
    //double check_angle = get_angle(U[1][0]) + mu - global_phase - M_PI;
    // cout << check_angle << endl;
    // if (abs(sin(check_angle)) > 1e-6 || cos(check_angle) < 1 - 1e-6) {
    //     cerr << "|U| is not e^it" << endl;
    //     bloch.clear();   // |U| is not e^it
    // }

    return bloch;
}

vector<string> cu_decompose(vector<vector<complex<double>>>& U, int targit_b, int ctrl_b) {
    vector<string> ckt(7);
    vector<double> U_bloch = to_bloch(U);
    if (U_bloch.empty()) {
        // cerr << "not SU" << endl;
        ckt.clear();
        ckt.push_back("cu q[" + to_string(ctrl_b) + "], q[" + to_string(targit_b) + "];\n");
        return ckt;
    }
    double theta = U_bloch[0];
    double lambda = U_bloch[1];
    double mu = U_bloch[2];

    ckt[0] = "rz(" + to_string(-mu) +") q[" + to_string(targit_b) + "];"+ "\n";
    ckt[1] = "cx q[" + to_string(ctrl_b) + "], q[" + to_string(targit_b) + "];\n";
    ckt[2] = "rz(" + to_string(-lambda) +") q[" + to_string(targit_b) + "];\n";
    ckt[3] = "ry(" + to_string(-theta) +") q[" + to_string(targit_b) + "];\n";
    ckt[4] = "cx q[" + to_string(ctrl_b) + "], q[" + to_string(targit_b) + "];\n";
    ckt[5] = "ry(" + to_string(theta) +") q[" + to_string(targit_b) + "];\n";
    ckt[6] = "rz(" + to_string(lambda + mu) +") q[" + to_string(targit_b) + "];" + "\n";

    return ckt;
}

vector<string> cnu_decompose(vector<vector<complex<double>>> U, int target_bits, int qubit) {
    vector<vector<complex<double>>> U_dag;
    vector<string> result;
    vector<string> cu_buff;
    string temp, temp2;
    int n = qubit - 1;
    for(int i = 0; i < qubit; i++){
        assert(n > 0);
        if(i != target_bits){
            // print_matrix(U);
            //if n = 1;
            if(n == 1){
                cu_buff = cu_decompose(U, target_bits, i);
                result.insert(result.end(), cu_buff.begin(), cu_buff.end());
                n--;
                break;
            }

            //first CV
            complex<double> s = sqrt(U[0][0]*U[1][1] - U[0][1]*U[1][0]); 
            complex<double> t = sqrt(U[0][0] + U[1][1] + (2.0 * s));
            U[0][0] = (U[0][0] + s)/t;
            U[0][1] = (U[0][1])/t;
            U[1][0] = (U[1][0])/t;
            U[1][1] = (U[1][1] + s)/t;
            // print_matrix(U);
            cu_buff = cu_decompose(U, target_bits, i);
            result.insert(result.end(), cu_buff.begin(), cu_buff.end());

            //second Cn-1X
            // temp = "c" + to_string(n-1) + "x";
            temp = "mcx ";
            for(int j = i+1; j < qubit; j++){
                if(j != target_bits){
                    temp = temp + "q[" +to_string(j) + "], ";
                }
            }
            temp = temp + "q[" +to_string(target_bits) + "];\n";
            result.push_back(temp);

            //third CV_dag
            U_dag = transposeMatrix(U);
            conjugateMatrix(U_dag);
            cu_buff = cu_decompose(U, target_bits, i);  // U_dag perhaps?
            result.insert(result.end(), cu_buff.begin(), cu_buff.end()); 


            // fourth Cn-1X
            result.push_back(temp);
            n--;
        }
    }
    result.push_back("--end cnu--\n");
    return result;
}
    
vector<vector<complex<double>>> to_2level(vector<vector<complex<double>>>& U, int& i, int& j) {
    complex<double> one;
    one.real(1);
    one.imag(0);
    vector<vector<complex<double>>> U2(2, vector<complex<double>>(2, 0.0));
    i = U.size() - 1;
    while (i > -1){
        // cout << U[i][i] << endl;
        if (abs(U[i][i] - one) > 1e-6) break;
        --i;
    }
    if (i == -1) {
        cerr << "incorrect matrix" << endl;
        return U2;
    }
    //complex<double> uii = U[i][i];
    j = 0;
    while (j < i){
        if (abs(U[j][i]) > 1e-6) break;
        ++j;
    }
    if (j == i) {
        cerr << "incorrect matrix" << endl;
        return U2;
    }

    U2[0][0] = U[j][j];
    U2[0][1] = U[j][i];
    U2[1][0] = U[i][j];
    U2[1][1] = U[i][i];
    return U2;
}

string str_q(int b) {
    return "q[" + to_string(b) + "]";
}

vector<string> vecstr_Ctrl(int b, int n, vector<vector<complex<double>>>& U2, vector<bool>& i_state) {
    vector<string> half_ckt;
    vector<string> cnU;
    for (int ctrl_b = 0; ctrl_b < n; ++ctrl_b) {
        if (ctrl_b == b) continue;
        if (ctrl_b >= int(i_state.size()) || i_state[ctrl_b] == 0) {
            half_ckt.push_back("rx(pi) " + str_q(ctrl_b) + ";\n");
        }
    }

    if (U2.empty()) {   //cnx -- turn x into stringU
        // string cnx = "c" + to_string(n-1) + "x ";
        string cnx = "mcx ";
        if((n-1) == 1) {
            cnx = "cx ";
        }
        for (int ctrl_b = 0; ctrl_b < n; ++ctrl_b) {
            if (ctrl_b == b) continue;
            cnx += str_q(ctrl_b) + ", ";
        }
        cnx += str_q(b) + ";\n";
        cnU.push_back(cnx);
    } else {
        cnU = cnu_decompose(U2, b, n);
    }

    vector<string> full_ckt = half_ckt;
    full_ckt.insert(full_ckt.end(), cnU.begin(), cnU.end());
    reverse(half_ckt.begin(), half_ckt.end());
    full_ckt.insert(full_ckt.end(), half_ckt.begin(), half_ckt.end());
    return full_ckt;
}

vector<string> gray_code(int i, int j, int n, vector<vector<complex<double>>>& U2){
    //cout << i << " " << j << endl;
    assert(i != j);
    vector<string> half_ckt;
    vector<bool> i_state, j_state;
    vector<vector<complex<double>>> dummy(0); //dummy indicate x
    while (i != 0) {
        i_state.push_back(false);
        if (i % 2) i_state[i_state.size() - 1] = true;
        i = i / 2;
    }
    while (j != 0) {
        j_state.push_back(false);
        if (j % 2) j_state[j_state.size() - 1] = true;
        j = j / 2;
    }
    while (i_state.size() != j_state.size()) {
        if (i_state.size() < j_state.size()) i_state.push_back(false);
        else j_state.push_back(false);
    }

    int U_b = -1;
    for (size_t b = 0; b < i_state.size(); ++b) {
        if (i_state[b] != j_state[b]) { // q[b] is flip
            if (U_b < 0) {
                U_b = b;
                continue;
            }
            vector<string> cnx = vecstr_Ctrl(b, n, dummy, i_state);
            half_ckt.insert(half_ckt.end(), cnx.begin(), cnx.end());
            i_state[b] = !i_state[b];
        }
    }
    assert(U_b != -1);
    vector<string> cnU = vecstr_Ctrl(U_b, n, U2, i_state);
    vector<string> full_ckt = half_ckt;
    full_ckt.insert(full_ckt.end(),cnU.begin(), cnU.end());
    reverse(half_ckt.begin(), half_ckt.end());
    full_ckt.insert(full_ckt.end(), half_ckt.begin(), half_ckt.end());
    return full_ckt;
}

void decompose(string input, string output){
    //輸入matrix大小
    ifstream fin(input);
    ofstream fout(output);
    int n;
    fin>>n;
    //輸入matrix
    vector<vector<complex<double>>> input_matrix(n, vector<complex<double>>(n, 0.0));
    string str;
    for(int i = 0; i < n; i++){
        for(int j = 0; j < n; j++){
            fin>>str;
            str = str.substr(1, str.size()-2);
            input_matrix[i][j].real(stod(str.substr(0, str.find(","))));
            input_matrix[i][j].imag(stod(str.substr(str.find(",")+1)));
            //cout<<"("<<input_matrix[n][n].real<<","<<input_matrix[n][n].imag<<") ";
        }
        //cout<<endl;
    }
    
    //check unitary
    assert(isUnitaryMatrix(input_matrix));
    
    //2-level decomposition
    vector<vector<vector<complex<double>>>> two_level_matrices;
    bool finish = 1, improve = 0;

    //check need to be decompose
    for(int i = 0; i < n; i++){
        if(fabs(abs(input_matrix[i][i]) - 1.0) > 1e-6){//|Mii| != 1
            finish = 0;
        }
    }

    while(!finish){
        //init
        improve = 0;
        vector<vector<complex<double>>> two_level_matrix(n, vector<complex<double>>(n, 0.0));
        for(int i = 0; i < n; i++){
            two_level_matrix[i][i] = 1;
        }

        //find Mii^2 + Mij^2 = 1 first
        for(int i = 0; i < n; i++){
            if(fabs(abs(input_matrix[i][i]) - 1.0) > 1e-6){//|Mii| != 1
                for(int j = 0; j < n; j++){
                    if(i != j){
                        if(fabs(sqrt(norm(input_matrix[i][i]) + norm(input_matrix[j][i])) - 1.0) < 1e-6){//Mii^2 + Mij^2 = 1
                            improve = 1;
                            //create two level matrix
                            two_level_matrix[i][i] = conj(input_matrix[i][i]);
                            two_level_matrix[i][j] = conj(input_matrix[j][i]);
                            two_level_matrix[j][i] = -input_matrix[j][i];
                            two_level_matrix[j][j] = input_matrix[i][i];

                            //multiply U
                            input_matrix = matrixMultiply(two_level_matrix, input_matrix);
                            
                            //for debug
                            // cout<<"2-level\n";
                            // for(int n = 0; n < two_level_matrix.size(); n++){
                            //     for(int k = 0; k < two_level_matrix[0].size(); k++){
                            //         cout<<two_level_matrix[n][k]<<" ";
                            //     }
                            //     cout<<endl;
                            // }
                            // cout<<endl;
                            

                            //dag and push back
                            conjugateMatrix(two_level_matrix);
                            two_level_matrices.push_back(transposeMatrix(two_level_matrix));
                            break;
                        }
                    } 
                }
            }
            if(improve){
                break;
            }
        }


        if(!improve){
            for(int i = 0; i < n-1; i++){
                if(fabs(abs(input_matrix[i][i]) - 1.0) > 1e-6){//|Mii| != 1
                    for(int j = 0; j < n; j++){
                        if((i != j) && fabs(abs(input_matrix[i][j]) - 1.0) > 1e-6){
                            improve = 1;
                            //create two level matrix
                            two_level_matrix[i][i] = conj(input_matrix[i][i])/sqrt(norm(input_matrix[i][i]) + norm(input_matrix[j][i]));
                            two_level_matrix[i][j] = conj(input_matrix[j][i])/sqrt(norm(input_matrix[i][i]) + norm(input_matrix[j][i]));
                            two_level_matrix[j][i] = -input_matrix[j][i]/sqrt(norm(input_matrix[i][i]) + norm(input_matrix[j][i]));
                            two_level_matrix[j][j] = input_matrix[i][i]/sqrt(norm(input_matrix[i][i]) + norm(input_matrix[j][i]));

                            //multiply U
                            input_matrix = matrixMultiply(two_level_matrix, input_matrix);

                            //dag and push back
                            conjugateMatrix(two_level_matrix);
                            two_level_matrices.push_back(transposeMatrix(two_level_matrix));
                            break;
                        }
                    }
                }
                if(improve){
                    break;
                }
            }
        }

        finish = 1;
        for(int i = 0; i < n; i++){
            if(fabs(abs(input_matrix[i][i]) - 1.0) > 1e-6){
                finish = 0;
            }
        }
        
        // for debug
        // cout<<"U : "<<endl;
        // for(int j = 0; j < input_matrix.size(); j++){
        //     for(int k = 0; k < input_matrix[j].size(); k++){
        //         cout<<input_matrix[j][k]<<" ";
        //     }
        //     cout<<endl;
        // }
        // cout<<endl;
    }
    
    two_level_matrices[two_level_matrices.size()-1] = matrixMultiply(two_level_matrices[two_level_matrices.size()-1], input_matrix);

    /*//for debug
    for(int i = 0; i < two_level_matrices.size(); i++){
        cout<<"matrix "<<(i+1)<<":"<<endl;
        for(int j = 0; j < two_level_matrices[i].matrix.size(); j++){
            for(int k = 0; k < two_level_matrices[i].matrix[j].size(); k++){
                cout<<two_level_matrices[i].matrix[j][k]<<" ";
            }
            cout<<endl;
        }
        cout<<endl;
    }*/

    fout<<"OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q["<<int(log2(n))<<"];\n\n";

    
    //gray-code
    for(size_t t = 0; t < two_level_matrices.size(); t++) {
        vector<vector<complex<double>>> U2;
        int i, j;
        U2 = to_2level(two_level_matrices[t], i, j);
        // vector<string> str_U2 = cu_decompose(U2, i, j);
        // print_matrix(U2);
        
        vector<string> str_U2 = gray_code(i,j,(int(log2(n))),U2);
        for (size_t s = 0; s < str_U2.size(); s++) {
            fout << str_U2[s];
        }
    }
    
    /*//cnu testcase
    vector<vector<complex<double>>> U(2, vector<complex<double>>(2, 0.0));
    U[0][1] = 1;
    U[1][0] = 1;

    int qubit = int(log2(n));
    //cnu decompose
    cout<<"\n\n";
    vector<string> cnu_gateset = cnu_decompose(U, 1, qubit);
    for(int i = 0; i < cnu_gateset.size(); i++){
        cout<<cnu_gateset[i];
    }*/
}

dvlab::Command qcir_decompose_cmd() {
    return {"unitary-decompose",
            [&](ArgumentParser& parser) {
                parser.description("decompose a unitary matrix");

                parser.add_argument<string>("fin")
                    .help("the input filename");
                parser.add_argument<string>("fout")
                    .help("the output filename");
            },
            [&](ArgumentParser const& parser) {
                decompose(parser.get<string>("fin"), parser.get<string>("fout"));
                return CmdExecResult::done;
            }};
}

Command qcir_cmd(QCirMgr& qcir_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(qcir_mgr);

    cmd.add_subcommand(dvlab::utils::mgr_list_cmd(qcir_mgr));
    cmd.add_subcommand(dvlab::utils::mgr_checkout_cmd(qcir_mgr));
    cmd.add_subcommand(dvlab::utils::mgr_new_cmd(qcir_mgr));
    cmd.add_subcommand(dvlab::utils::mgr_delete_cmd(qcir_mgr));
    cmd.add_subcommand(dvlab::utils::mgr_copy_cmd(qcir_mgr));
    cmd.add_subcommand(qcir_config_cmd());
    cmd.add_subcommand(qcir_compose_cmd(qcir_mgr));
    cmd.add_subcommand(qcir_tensor_product_cmd(qcir_mgr));
    cmd.add_subcommand(qcir_read_cmd(qcir_mgr));
    cmd.add_subcommand(qcir_write_cmd(qcir_mgr));
    cmd.add_subcommand(qcir_print_cmd(qcir_mgr));
    cmd.add_subcommand(qcir_draw_cmd(qcir_mgr));
    cmd.add_subcommand(qcir_gate_cmd(qcir_mgr));
    cmd.add_subcommand(qcir_qubit_cmd(qcir_mgr));
    cmd.add_subcommand(qcir_optimize_cmd(qcir_mgr));
    cmd.add_subcommand(qcir_decompose_cmd());
    return cmd;
}

bool add_qcir_cmds(dvlab::CommandLineInterface& cli, QCirMgr& qcir_mgr) {
    if (!cli.add_command(qcir_cmd(qcir_mgr))) {
        spdlog::error("Registering \"qcir\" commands fails... exiting");
        return false;
    }
    return true;
}

}  // namespace qsyn::qcir
