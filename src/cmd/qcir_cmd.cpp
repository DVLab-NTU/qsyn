/****************************************************************************
  PackageName  [ qcir ]
  Synopsis     [ Define qcir package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "cmd/qcir_cmd.hpp"

#include <fmt/color.h>
#include <fmt/ostream.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include "./qcir/optimizer_cmd.hpp"
#include "./qcir/oracle_cmd.hpp"
#include "argparse/arg_parser.hpp"
#include "argparse/arg_type.hpp"
#include "cli/cli.hpp"
#include "cmd/qcir_mgr.hpp"
#include "qcir/basic_gate_type.hpp"
#include "qcir/qcir.hpp"
#include "qcir/qcir_equiv.hpp"
#include "qcir/qcir_gate.hpp"
#include "qcir/qcir_io.hpp"
#include "qcir/qcir_translate.hpp"
#include "util/cin_cout_cerr.hpp"
#include "util/data_structure_manager_common_cmd.hpp"
#include "util/dvlab_string.hpp"
#include "util/phase.hpp"
#include "util/text_format.hpp"
#include "util/util.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::qcir {

std::function<bool(size_t const&)> valid_qcir_id(QCirMgr const& qcir_mgr) {
    return [&](size_t const& id) {
        if (qcir_mgr.get() && qcir_mgr.is_id(id))
            return true;
        spdlog::error("QCir {} does not exist!!", id);
        return false;
    };
}

std::function<bool(size_t const&)>
valid_qcir_gate_id(QCirMgr const& qcir_mgr) {
    return [&](size_t const& id) {
        if (!dvlab::utils::mgr_has_data(qcir_mgr))
            return false;
        if (qcir_mgr.get() && qcir_mgr.get()->get_gate(id) != nullptr)
            return true;
        spdlog::error("Gate ID {} does not exist!!", id);
        return false;
    };
}

std::function<bool(QubitIdType const&)>
valid_qcir_qubit_id(QCirMgr const& qcir_mgr) {
    return [&](QubitIdType const& id) {
        if (!dvlab::utils::mgr_has_data(qcir_mgr))
            return false;
        if (qcir_mgr.get() && id < qcir_mgr.get()->get_num_qubits())
            return true;
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
                if (!dvlab::utils::mgr_has_data(qcir_mgr))
                    return CmdExecResult::error;
                qcir_mgr.get()->compose(
                    *qcir_mgr.find_by_id(parser.get<size_t>("id")));
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
                if (!dvlab::utils::mgr_has_data(qcir_mgr))
                    return CmdExecResult::error;
                qcir_mgr.get()->tensor_product(
                    *qcir_mgr.find_by_id(parser.get<size_t>("id")));
                return CmdExecResult::done;
            }};
}

dvlab::Command qcir_read_cmd(QCirMgr& qcir_mgr) {
    return {
        "read",
        [](ArgumentParser& parser) {
            parser.description(
                "read a circuit and construct the corresponding netlist");

            parser.add_argument<std::string>("filepath")
                .constraint(path_readable)
                .constraint(
                    allowed_extension({".qasm", ".qc"}))
                .help(
                    "the filepath to quantum circuit file. Supported extension: "
                    ".qasm, .qc");

            parser.add_argument<bool>("-r", "--replace")
                .action(store_true)
                .help(
                    "if specified, replace the current circuit; otherwise store "
                    "to a new one");
        },
        [&](ArgumentParser const& parser) {
            auto const filepath = parser.get<std::string>("filepath");
            auto replace        = parser.get<bool>("--replace");
            auto qcir           = from_file(filepath);
            if (!qcir) {
                fmt::println("Error: the format in \"{}\" has something wrong!!",
                             filepath);
                return CmdExecResult::error;
            }
            if (qcir_mgr.empty() || !replace) {
                qcir_mgr.add(qcir_mgr.get_next_id(),
                             std::make_unique<QCir>(std::move(*qcir)));
            } else {
                qcir_mgr.set(std::make_unique<QCir>(std::move(*qcir)));
            }
            qcir_mgr.get()->set_filename(std::filesystem::path{filepath}.stem());
            return CmdExecResult::done;
        }};
}

dvlab::Command qcir_write_cmd(QCirMgr const& qcir_mgr) {
    return {
        "write",
        [](ArgumentParser& parser) {
            parser.description("write QCir to a QASM file");

            parser.add_argument<std::string>("output_path")
                .nargs(NArgsOption::optional)
                .constraint(path_writable)
                .constraint(allowed_extension({".qasm"}))
                .help(
                    "the filepath to output file. Supported extension: .qasm. If "
                    "not specified, the result will be dumped to the terminal");

            parser.add_argument<std::string>("-f", "--format")
                .constraint(choices_allow_prefix({"qasm", "latex-qcircuit"}))
                .help(
                    "the output format of the QCir. If not specified, the "
                    "default format is automatically chosen based on the output "
                    "file extension");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(qcir_mgr))
                return CmdExecResult::error;

            enum class OutputFormat : std::uint8_t {
                qasm,
                latex_qcircuit
            };
            auto output_type = std::invoke([&]() -> OutputFormat {
                if (parser.parsed("--format")) {
                    if (dvlab::str::is_prefix_of(parser.get<std::string>("--format"),
                                                 "qasm"))
                        return OutputFormat::qasm;
                    if (dvlab::str::is_prefix_of(parser.get<std::string>("--format"),
                                                 "latex-qcircuit"))
                        return OutputFormat::latex_qcircuit;
                    // if (dvlab::str::is_prefix_of(parser.get<std::string>("--format"),
                    // "latex-yquant")) return OutputFormat::latex_yquant;
                    DVLAB_UNREACHABLE("Invalid output format!!");
                }

                auto extension =
                    std::filesystem::path{parser.get<std::string>("output_path")}
                        .extension()
                        .string();
                if (extension == ".qasm")
                    return OutputFormat::qasm;
                if (extension == ".tex")
                    return OutputFormat::latex_qcircuit;
                return OutputFormat::qasm;
            });

            switch (output_type) {
                case OutputFormat::qasm:
                    if (parser.parsed("output_path")) {
                        std::ofstream file{parser.get<std::string>("output_path")};
                        if (!file) {
                            spdlog::error("Path {} not found!!",
                                          parser.get<std::string>("output_path"));
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
    return {
        "draw",
        [](ArgumentParser& parser) {
            parser.description(
                "draw a QCir. This command relies on qiskit and "
                "pdflatex to be present in the system");

            parser.add_argument<std::string>("output-path")
                .constraint(path_writable)
                .help("the output destination of the drawing");
            parser.add_argument<std::string>("-d", "--drawer")
                .choices(std::initializer_list<std::string>{"text", "mpl", "latex"})
                .default_value("text")
                .help(
                    "the backend for drawing quantum circuit. If not specified, "
                    "the default backend is automatically chosen based on the "
                    "output file extension");
            parser.add_argument<float>("-s", "--scale")
                .default_value(1.0f)
                .help("if specified, scale the resulting drawing by this factor");
        },
        [&](ArgumentParser const& parser) {
            namespace fs = std::filesystem;
            if (!dvlab::utils::mgr_has_data(qcir_mgr))
                return CmdExecResult::error;

            auto output_path = fs::path{parser.get<std::string>("output-path")};
            auto scale       = parser.get<float>("--scale");
            auto drawer      = std::invoke([&]() -> QCirDrawerType {
                if (parser.parsed("--drawer"))
                    return *qcir::str_to_qcir_drawer_type(
                        parser.get<std::string>("--drawer"));

                auto extension = output_path.extension().string();
                if (extension == ".pdf" || extension == ".png" ||
                    extension == ".jpg" || extension == ".ps" ||
                    extension == ".eps" || extension == ".svg")
                    return QCirDrawerType::latex;
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
    return {
        "print",
        [](ArgumentParser& parser) {
            parser.description("print info of QCir");

            parser.add_argument<bool>("-v", "--verbose")
                .action(store_true)
                .help("display more information");

            auto mutex = parser.add_mutually_exclusive_group();

            mutex.add_argument<bool>("-s", "--statistics")
                .action(store_true)
                .help(
                    "print gate statistics of the circuit. When `--verbose` is "
                    "also specified, print more detailed gate counts");
            mutex.add_argument<size_t>("-g", "--gate")
                .nargs(NArgsOption::zero_or_more)
                .help(
                    "print information for the gates with the specified IDs. If "
                    "the ID is not specified, print all gates. When `--verbose` "
                    "is also specified, print the gates' predecessor and "
                    "successor gates");
            mutex.add_argument<bool>("-d", "--diagram")
                .action(store_true)
                .help(
                    "print the circuit diagram. If `--verbose` is also "
                    "specified, print the circuit diagram in the qiskit style");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(qcir_mgr)) {
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
                fmt::println("Depth      : {}", qcir_mgr.get()->calculate_depth());
            } else {
                qcir_mgr.get()->print_qcir_info();
            }

            return CmdExecResult::done;
        }};
}

dvlab::Command qcir_gate_add_cmd(QCirMgr& qcir_mgr) {
    static dvlab::utils::ordered_hashmap<std::string, std::string> const
        single_qubit_gates_no_phase = {
            {"h", "Hadamard gate"}, {"x", "Pauli-X gate"}, {"y", "Pauli-Y gate"}, {"z", "Pauli-Z gate"}, {"t", "T gate"}, {"tdg", "T† gate"}, {"s", "S gate"}, {"sdg", "S† gate"}, {"sx", "√X gate"}, {"sy", "√Y gate"}};

    static dvlab::utils::ordered_hashmap<std::string, std::string> const
        single_qubit_gates_with_phase = {
            {"rz", "Rz(θ) gate"}, {"ry", "Rx(θ) gate"}, {"rx", "Ry(θ) gate"}, {"p", "P = (e^iθ/2)Rz gate"}, {"pz", "Pz = (e^iθ/2)Rz gate"}, {"px", "Px = (e^iθ/2)Rx gate"}, {"py", "Py = (e^iθ/2)Ry gate"}  //
        };

    static dvlab::utils::ordered_hashmap<std::string, std::string> const
        double_qubit_gates_no_phase = {{"cx", "CX (CNOT) gate"},
                                       {"cz", "CZ gate"},
                                       {"swap", "SWAP gate"},
                                       {"ecr", "Echoed crossed resonance gate"}};

    static dvlab::utils::ordered_hashmap<std::string, std::string> const
        three_qubit_gates_no_phase = {
            {"ccx", "CCX (CCNOT, Toffoli) gate"}, {"ccz", "CCZ gate"}  //
        };

    static dvlab::utils::ordered_hashmap<std::string, std::string> const
        multi_qubit_gates_with_phase = {
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
                "For control gates, the control qubits comes "
                "before the target qubits.";

            for (auto& category :
                 {single_qubit_gates_no_phase, single_qubit_gates_with_phase,
                  double_qubit_gates_no_phase, three_qubit_gates_no_phase,
                  multi_qubit_gates_with_phase}) {
                for (auto& [name, help] : category) {
                    type_choices.emplace_back(name);
                    type_help += '\n' + name + ": ";
                    if (name.size() < 4)
                        type_help += std::string(4 - name.size(), ' ');
                    type_help += help;
                }
            }
            parser.add_argument<std::string>("type").help(type_help);

            auto append_or_prepend =
                parser.add_mutually_exclusive_group().required(false);
            append_or_prepend.add_argument<bool>("--append")
                .help("append the gate at the end of QCir")
                .action(store_true);
            append_or_prepend.add_argument<bool>("--prepend")
                .help("prepend the gate at the start of QCir")
                .action(store_true);

            parser.add_argument<dvlab::Phase>("-ph", "--phase")
                .help(
                    "The rotation angle θ. This option must be specified if and "
                    "only if the gate type takes a phase parameter.");

            parser.add_argument<QubitIdType>("qubits")
                .nargs(NArgsOption::zero_or_more)
                .constraint(valid_qcir_qubit_id(qcir_mgr))
                .help("the qubits on which the gate applies");
        },
        [=, &qcir_mgr](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(qcir_mgr))
                return CmdExecResult::error;
            bool const do_prepend = parser.parsed("--prepend");

            auto const type = dvlab::str::tolower_string(parser.get<std::string>("type"));
            auto const bits = parser.get<QubitIdList>("qubits");

            if (!QCirGate::qubit_id_is_unique(bits)) {
                spdlog::error("Qubits must be unique!!");
                return CmdExecResult::error;
            }

            if (type.starts_with("mc")) {
                auto const op = str_to_operation(
                    type.substr(2), parser.get<std::vector<dvlab::Phase>>("--phase"));
                if (!op.has_value()) {
                    spdlog::error("Invalid gate type {}!!", type);
                    return CmdExecResult::error;
                }
                if (bits.size() < op->get_num_qubits()) {
                    spdlog::error("Too few qubits are supplied for gate {}!!", type);
                    return CmdExecResult::error;
                }
                auto const n_ctrls = bits.size() - op->get_num_qubits();
                if (n_ctrls > 0) {
                    do_prepend
                        ? qcir_mgr.get()->prepend(ControlGate(*op, n_ctrls), bits)
                        : qcir_mgr.get()->append(ControlGate(*op, n_ctrls), bits);
                } else {
                    do_prepend ? qcir_mgr.get()->prepend(*op, bits)
                               : qcir_mgr.get()->append(*op, bits);
                }
            } else {
                auto const op = str_to_operation(
                    type, parser.get<std::vector<dvlab::Phase>>("--phase"));
                if (!op.has_value()) {
                    spdlog::error("Invalid gate type {}!!", type);
                    return CmdExecResult::error;
                }
                if (bits.size() < op->get_num_qubits()) {
                    spdlog::error("Too few qubits are supplied for gate {}!!", type);
                    return CmdExecResult::error;
                } else if (bits.size() > op->get_num_qubits()) {
                    spdlog::error("Too many qubits are supplied for gate {}!!", type);
                    return CmdExecResult::error;
                }
                do_prepend ? qcir_mgr.get()->prepend(*op, bits)
                           : qcir_mgr.get()->append(*op, bits);
            }

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
                if (!dvlab::utils::mgr_has_data(qcir_mgr))
                    return CmdExecResult::error;
                qcir_mgr.get()->remove_gate(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

dvlab::Command qcir_gate_cmd(QCirMgr& qcir_mgr) {
    auto cmd = Command{
        "gate",
        [](ArgumentParser& parser) {
            parser.description("gate commands");

            parser.add_subparsers("gate-cmd").required(true);
        },
        [](ArgumentParser const& /*parser*/) { return CmdExecResult::error; }};

    cmd.add_subcommand("gate-cmd", qcir_gate_add_cmd(qcir_mgr));
    cmd.add_subcommand("gate-cmd", qcir_gate_delete_cmd(qcir_mgr));

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

                qcir_mgr.get()->add_qubits(
                    parser.parsed("n") ? parser.get<size_t>("n") : 1);
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
                if (!dvlab::utils::mgr_has_data(qcir_mgr))
                    return CmdExecResult::error;
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

            parser.add_subparsers("qubit-cmd").required(true);
        },
        [](ArgumentParser const& /*parser*/) { return CmdExecResult::error; }};

    cmd.add_subcommand("qubit-cmd", qcir_qubit_add_cmd(qcir_mgr));
    cmd.add_subcommand("qubit-cmd", qcir_qubit_delete_cmd(qcir_mgr));

    return cmd;
}

dvlab::Command qcir_adjoint_cmd(QCirMgr& qcir_mgr) {
    return {"adjoint",
            [](ArgumentParser& parser) {
                parser.description(
                    "transform the QCir to its adjoint, i.e., reverse the order of "
                    "gates and replace each gate with its adjoint version");
            },
            [&](ArgumentParser const& /*parser*/) {
                if (!dvlab::utils::mgr_has_data(qcir_mgr))
                    return CmdExecResult::error;
                qcir_mgr.get()->adjoint_inplace();
                return CmdExecResult::done;
            }};
}

dvlab::Command qcir_translate_cmd(QCirMgr& qcir_mgr) {
    return {"translate",
            [&](ArgumentParser& parser) {
                parser.description(
                    "translate the circuit into a specific gate set");
                parser.add_argument<std::string>("gate_set")
                    .help("the specific gate set ('sherbrooke', 'kyiv', 'prague')")
                    .choices(std::initializer_list<std::string>{"sherbrooke",
                                                                "kyiv", "prague"});
            },
            [=, &qcir_mgr](ArgumentParser const& parser) {
                auto const gate_set  = parser.get<std::string>("gate_set");
                auto translated_qcir = translate(*qcir_mgr.get(), gate_set);
                if (!translated_qcir) {
                    spdlog::error("Translation fails!!");
                    return CmdExecResult::error;
                }
                std::string const filename = qcir_mgr.get()->get_filename();
                qcir_mgr.set(std::make_unique<QCir>(*std::move(translated_qcir)));
                qcir_mgr.get()->set_filename(filename);
                return CmdExecResult::done;
            }};
}

Command qcir_equiv_cmd(QCirMgr& qcir_mgr) {
    return {
        "equiv",
        [&](ArgumentParser& parser) {
            parser.description(
                "check if two circuits are equivalent. A Tableau-based "
                "method is used to check the equivalence. If that fails, "
                "and the circuits are small enough, also verify the "
                "equivalence are through tensor calculation.");

            parser.add_argument<size_t>("ids")
                .nargs(1, 2)
                .constraint(valid_qcir_id(qcir_mgr))
                .help("Compare the two QCirs. If only one is specified, compare with the QCir in focus");
        },
        [&](ArgumentParser const& parser) {
            if (!dvlab::utils::mgr_has_data(qcir_mgr))
                return CmdExecResult::error;

            auto const ids = parser.get<std::vector<size_t>>("ids");

            if ((ids.size() == 2 && ids[0] == ids[1]) ||
                (ids.size() == 1 && qcir_mgr.focused_id() == ids[0])) {
                spdlog::info("Note: comparing the same circuit...");
            }
            auto const is_equiv = [&]() -> bool {
                if (ids.size() == 1) {
                    return is_equivalent(
                        *qcir_mgr.get(),
                        *qcir_mgr.find_by_id(ids[0]));
                } else {
                    return is_equivalent(
                        *qcir_mgr.find_by_id(ids[0]),
                        *qcir_mgr.find_by_id(ids[1]));
                }
            }();

            if (is_equiv) {
                fmt::println(
                    "{}",
                    dvlab::fmt_ext::styled_if_ansi_supported(
                        "The two circuits are equivalent!!",
                        fmt::fg(fmt::terminal_color::green) | fmt::emphasis::bold));
            } else {
                fmt::println(
                    "{}",
                    dvlab::fmt_ext::styled_if_ansi_supported(
                        "The two circuits are not equivalent!!",
                        fmt::fg(fmt::terminal_color::red) | fmt::emphasis::bold));
            }

            return CmdExecResult::done;
        }};
};

Command qcir_to_basic_cmd(QCirMgr& qcir_mgr);

Command qcir_cmd(QCirMgr& qcir_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(qcir_mgr);

    cmd.add_subcommand("qcir-cmd-group", dvlab::utils::mgr_list_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group",
                       dvlab::utils::mgr_checkout_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", dvlab::utils::mgr_new_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", dvlab::utils::mgr_delete_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", dvlab::utils::mgr_copy_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_compose_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_tensor_product_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_read_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_write_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_print_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_draw_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_adjoint_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_gate_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_qubit_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_optimize_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_translate_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_oracle_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_equiv_cmd(qcir_mgr));
    cmd.add_subcommand("qcir-cmd-group", qcir_to_basic_cmd(qcir_mgr));
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
