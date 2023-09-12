/****************************************************************************
  PackageName  [ zx ]
  Synopsis     [ Define zx package commands ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_cmd.hpp"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <string>

#include "./to_tensor.hpp"
#include "./zxgraph_mgr.hpp"
#include "tensor/tensor_mgr.hpp"
#include "zx/zx_def.hpp"

using namespace std;

ZXGraphMgr ZXGRAPH_MGR{"ZXGraph"};
extern TensorMgr TENSOR_MGR;
using namespace argparse;

Command zxgraph_checkout_cmd();
Command zxgraph_new_cmd();
Command zxgraph_mgr_reset_cmd();
Command zxgraph_delete_cmd();
Command zxgraph_mgr_print_cmd();
Command zxgraph_copy_cmd();
Command zxgraph_compose_cmd();
Command zxgraph_tensor_product_cmd();
Command zxgraph_traverse_cmd();
Command zxgraph_to_tensor_cmd();
Command zxgraph_adjoint_cmd();
Command zxgraph_test_cmd();
Command zxgraph_draw_cmd();
Command zxgraph_print_cmd();
Command zxgraph_edit_cmd();
Command zxgraph_read_cmd();
Command zxgraph_write_cmd();
Command zxgraph_assign_boundary_cmd();

bool add_zx_cmds() {
    if (!(CLI.add_command(zxgraph_checkout_cmd()) &&
          CLI.add_command(zxgraph_new_cmd()) &&
          CLI.add_command(zxgraph_mgr_reset_cmd()) &&
          CLI.add_command(zxgraph_delete_cmd()) &&
          CLI.add_command(zxgraph_copy_cmd()) &&
          CLI.add_command(zxgraph_compose_cmd()) &&
          CLI.add_command(zxgraph_tensor_product_cmd()) &&
          CLI.add_command(zxgraph_mgr_print_cmd()) &&
          CLI.add_command(zxgraph_print_cmd()) &&
          CLI.add_command(zxgraph_test_cmd()) &&
          CLI.add_command(zxgraph_edit_cmd()) &&
          CLI.add_command(zxgraph_adjoint_cmd()) &&
          CLI.add_command(zxgraph_assign_boundary_cmd()) &&
          CLI.add_command(zxgraph_traverse_cmd()) &&
          CLI.add_command(zxgraph_draw_cmd()) &&
          CLI.add_command(zxgraph_to_tensor_cmd()) &&
          CLI.add_command(zxgraph_read_cmd()) &&
          CLI.add_command(zxgraph_write_cmd()))) {
        cerr << "Registering \"zx\" commands fails... exiting" << endl;
        return false;
    }
    return true;
}

bool valid_zxgraph_id(size_t const& id) {
    if (ZXGRAPH_MGR.is_id(id)) return true;
    LOGGER.error("ZXGraph {} does not exist!!", id);
    return false;
};

bool zxgraph_id_not_exist(size_t const& id) {
    if (!ZXGRAPH_MGR.is_id(id)) return true;
    LOGGER.error("ZXGraph {} already exists!!", id);
    LOGGER.info("Use `-Replace` if you want to overwrite it.");
    return false;
};

bool valid_zxvertex_id(size_t const& id) {
    if (ZXGRAPH_MGR.get()->is_v_id(id)) return true;
    LOGGER.error("Cannot find vertex with ID {} in the ZXGraph!!", id);
    return false;
};

bool zxgraph_input_qubit_not_exist(size_t const& qid) {
    if (!ZXGRAPH_MGR.get()->is_input_qubit(qid)) return true;
    LOGGER.error("This qubit's input already exists!!");
    return false;
};

bool zxgraph_output_qubit_not_exist(size_t const& qid) {
    if (!ZXGRAPH_MGR.get()->is_output_qubit(qid)) return true;
    LOGGER.error("This qubit's output already exists!!");
    return false;
};

bool zxgraph_mgr_not_empty() {
    if (ZXGRAPH_MGR.empty()) {
        LOGGER.error("ZXGraph list is empty. Please create a ZXGraph first!!");
        LOGGER.info("Use ZXNew to add a new ZXGraph, or ZXGRead to read a ZXGraph from a file.");
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    ZXCHeckout <(size_t id)>
//----------------------------------------------------------------------
Command zxgraph_checkout_cmd() {
    return {"zxcheckout",
            [](ArgumentParser& parser) {
                parser.description("checkout to Graph <id> in ZXGraphMgr");
                parser.add_argument<size_t>("id")
                    .constraint(valid_zxgraph_id)
                    .help("the ID of the ZXGraph");
            },
            [](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                ZXGRAPH_MGR.checkout(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXNew [(size_t id)]
//----------------------------------------------------------------------
Command zxgraph_new_cmd() {
    return {"zxnew",
            [](ArgumentParser& parser) {
                parser.description("create a new ZXGraph to ZXGraphMgr");

                parser.add_argument<size_t>("id")
                    .nargs(NArgsOption::optional)
                    .help("the ID of the ZXGraph");

                parser.add_argument<bool>("-Replace")
                    .action(store_true)
                    .help("if specified, replace the current ZXGraph; otherwise store to a new one");
            },
            [](ArgumentParser const& parser) {
                size_t id = (parser.parsed("id")) ? parser.get<size_t>("id") : ZXGRAPH_MGR.get_next_id();

                if (ZXGRAPH_MGR.is_id(id)) {
                    if (!parser.parsed("-Replace")) {
                        LOGGER.error("ZXGraph {} already exists!! Specify `-Replace` if needed.", id);
                        return CmdExecResult::error;
                    }
                    ZXGRAPH_MGR.set(make_unique<ZXGraph>());
                    return CmdExecResult::done;
                } else {
                    ZXGRAPH_MGR.add(id);
                }

                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXReset
//----------------------------------------------------------------------
Command zxgraph_mgr_reset_cmd() {
    return {"zxreset",
            [](ArgumentParser& parser) {
                parser.description("reset ZXGraphMgr");
            },
            [](ArgumentParser const& /*parser*/) {
                ZXGRAPH_MGR.reset();
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXDelete <(size_t id)>
//----------------------------------------------------------------------
Command zxgraph_delete_cmd() {
    return {"zxdelete",
            [](ArgumentParser& parser) {
                parser.description("remove a ZXGraph from ZXGraphMgr");

                parser.add_argument<size_t>("id")
                    .constraint(valid_zxgraph_id)
                    .help("the ID of the ZXGraph");
            },
            [](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                ZXGRAPH_MGR.remove(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXPrint [-Summary | -Focus | -Num]
//----------------------------------------------------------------------
Command zxgraph_mgr_print_cmd() {
    return {"zxprint",
            [](ArgumentParser& parser) {
                parser.description("print info about ZXGraphs");
                auto mutex = parser.add_mutually_exclusive_group().required(false);

                mutex.add_argument<bool>("-focus")
                    .action(store_true)
                    .help("print the info of the ZXGraph in focus");
                mutex.add_argument<bool>("-list")
                    .action(store_true)
                    .help("print a list of ZXGraphs");
            },
            [](ArgumentParser const& parser) {
                if (parser.parsed("-focus"))
                    ZXGRAPH_MGR.print_focus();
                else if (parser.parsed("-list"))
                    ZXGRAPH_MGR.print_list();
                else
                    ZXGRAPH_MGR.print_manager();
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXCOPy [(size_t id)]
//----------------------------------------------------------------------
Command zxgraph_copy_cmd() {
    return {"zxcopy",
            [](ArgumentParser& parser) {
                parser.description("copy a ZXGraph to ZXGraphMgr");

                parser.add_argument<size_t>("id")
                    .nargs(NArgsOption::optional)
                    .help("the ID copied ZXGraph to be stored");

                parser.add_argument<bool>("-replace")
                    .action(store_true)
                    .help("replace the current focused ZXGraph");
            },
            [](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                size_t id = (parser.parsed("id")) ? parser.get<size_t>("id") : ZXGRAPH_MGR.get_next_id();
                if (ZXGRAPH_MGR.is_id(id)) {
                    if (!parser.parsed("-replace")) {
                        LOGGER.error("ZXGraph {} already exists!! Specify `-Replace` if needed.", id);
                        return CmdExecResult::error;
                    }
                    ZXGRAPH_MGR.copy(id);
                    return CmdExecResult::done;
                }

                ZXGRAPH_MGR.copy(id);
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXCOMpose <size_t id>
//----------------------------------------------------------------------
Command zxgraph_compose_cmd() {
    return {"zxcompose",
            [](ArgumentParser& parser) {
                parser.description("compose a ZXGraph");

                parser.add_argument<size_t>("id")
                    .constraint(valid_zxgraph_id)
                    .help("the ID of the ZXGraph to compose with");
            },
            [](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                ZXGRAPH_MGR.get()->compose(*ZXGRAPH_MGR.find_by_id(parser.get<size_t>("id")));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXTensor <size_t id>
//----------------------------------------------------------------------
Command zxgraph_tensor_product_cmd() {
    return {"zxtensor",
            [](ArgumentParser& parser) {
                parser.description("tensor a ZXGraph");

                parser.add_argument<size_t>("id")
                    .constraint(valid_zxgraph_id)
                    .help("the ID of the ZXGraph");
            },
            [](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                ZXGRAPH_MGR.get()->tensor_product(*ZXGRAPH_MGR.find_by_id(parser.get<size_t>("id")));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXGTest [-Empty | -Valid | -GLike | -IDentity]
//----------------------------------------------------------------------
Command zxgraph_test_cmd() {
    return {"zxgtest",
            [](ArgumentParser& parser) {
                parser.description("test ZXGraph structures and functions");

                auto mutex = parser.add_mutually_exclusive_group().required(true);

                mutex.add_argument<bool>("-empty")
                    .action(store_true)
                    .help("check if the ZXGraph is empty");
                mutex.add_argument<bool>("-valid")
                    .action(store_true)
                    .help("check if the ZXGraph is valid");
                mutex.add_argument<bool>("-glike")
                    .action(store_true)
                    .help("check if the ZXGraph is graph-like");
                mutex.add_argument<bool>("-identity")
                    .action(store_true)
                    .help("check if the ZXGraph is equivalent to identity");
            },
            [](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                if (parser.parsed("-empty")) {
                    if (ZXGRAPH_MGR.get()->is_empty())
                        cout << "The graph is empty!" << endl;
                    else
                        cout << "The graph is not empty!" << endl;
                } else if (parser.parsed("-valid")) {
                    if (ZXGRAPH_MGR.get()->is_valid())
                        cout << "The graph is valid!" << endl;
                    else
                        cout << "The graph is invalid!" << endl;
                } else if (parser.parsed("-glike")) {
                    if (ZXGRAPH_MGR.get()->is_graph_like())
                        cout << "The graph is graph-like!" << endl;
                    else
                        cout << "The graph is not graph-like!" << endl;
                } else if (parser.parsed("-identity")) {
                    if (ZXGRAPH_MGR.get()->is_identity())
                        cout << "The graph is an identity!" << endl;
                    else
                        cout << "The graph is not an identity!" << endl;
                }
                return CmdExecResult::done;
            }};
}

//-----------------------------------------------------------------------------------------------------------
//    ZXGPrint [-Summary | -Inputs | -Outputs | -Vertices | -Edges | -Qubits | -Neighbors | -Analysis | -Density]
//-----------------------------------------------------------------------------------------------------------
Command zxgraph_print_cmd() {
    return {"zxgprint",
            [](ArgumentParser& parser) {
                parser.description("print info of ZXGraph");

                auto mutex = parser.add_mutually_exclusive_group();
                mutex.add_argument<bool>("-list")
                    .action(store_true)
                    .help("print a list of ZXGraphs");
                mutex.add_argument<bool>("-summary")
                    .action(store_true)
                    .help("print the summary info of ZXGraph");
                mutex.add_argument<bool>("-io")
                    .action(store_true)
                    .help("print the I/O info of ZXGraph");
                mutex.add_argument<bool>("-inputs")
                    .action(store_true)
                    .help("print the input info of ZXGraph");
                mutex.add_argument<bool>("-outputs")
                    .action(store_true)
                    .help("print the output info of ZXGraph");
                mutex.add_argument<size_t>("-vertices")
                    .nargs(NArgsOption::zero_or_more)
                    .constraint(valid_zxvertex_id)
                    .help("print the vertex info of ZXGraph");
                mutex.add_argument<bool>("-edges")
                    .action(store_true)
                    .help("print the edges info of ZXGraph");
                mutex.add_argument<int>("-qubits")
                    .nargs(NArgsOption::zero_or_more)
                    .help("print the qubit info of ZXGraph");
                mutex.add_argument<size_t>("-neighbors")
                    .constraint(valid_zxvertex_id)
                    .help("print the neighbor info of ZXGraph");
                mutex.add_argument<bool>("-density")
                    .action(store_true)
                    .help("print the density of ZXGraph");
            },

            [](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                if (parser.parsed("-summary")) {
                    ZXGRAPH_MGR.get()->print_graph();
                    fmt::println("{:<29} {}", "#T-gate:", ZXGRAPH_MGR.get()->t_count());
                    fmt::println("{:<29} {}", "#Non-(Clifford+T)-gate: ", ZXGRAPH_MGR.get()->non_clifford_t_count());
                    fmt::println("{:<29} {}", "#Non-Clifford-gate: ", ZXGRAPH_MGR.get()->non_clifford_count());
                } else if (parser.parsed("-io"))
                    ZXGRAPH_MGR.get()->print_io();
                else if (parser.parsed("-list"))
                    ZXGRAPH_MGR.print_list();
                else if (parser.parsed("-inputs"))
                    ZXGRAPH_MGR.get()->print_inputs();
                else if (parser.parsed("-outputs"))
                    ZXGRAPH_MGR.get()->print_outputs();
                else if (parser.parsed("-vertices")) {
                    auto vids = parser.get<vector<size_t>>("-vertices");
                    if (vids.empty())
                        ZXGRAPH_MGR.get()->print_vertices();
                    else
                        ZXGRAPH_MGR.get()->print_vertices(vids);
                } else if (parser.parsed("-edges"))
                    ZXGRAPH_MGR.get()->print_edges();
                else if (parser.parsed("-qubits")) {
                    auto qids = parser.get<vector<int>>("-qubits");
                    ZXGRAPH_MGR.get()->print_qubits(qids);
                } else if (parser.parsed("-neighbors")) {
                    auto v = ZXGRAPH_MGR.get()->find_vertex_by_id(parser.get<size_t>("-neighbors"));
                    v->print_vertex();
                    cout << "----- Neighbors -----" << endl;
                    for (auto [nb, _] : v->get_neighbors()) {
                        nb->print_vertex();
                    }
                } else if (parser.parsed("-density")) {
                    cout << "Density: " << ZXGRAPH_MGR.get()->density() << endl;
                } else
                    ZXGRAPH_MGR.get()->print_graph();
                return CmdExecResult::done;
            }};
}

Command zxgraph_edit_cmd() {
    return {"zxgedit",
            [](ArgumentParser& parser) {
                parser.description("edit ZXGraph");

                auto subparsers = parser.add_subparsers().required(true);

                auto remove_vertex_parser = subparsers.add_parser("-rmvertex");

                auto rmv_mutex = remove_vertex_parser.add_mutually_exclusive_group().required(true);

                rmv_mutex.add_argument<size_t>("ids")
                    .constraint(valid_zxvertex_id)
                    .nargs(NArgsOption::zero_or_more)
                    .help("the IDs of vertices to remove");

                rmv_mutex.add_argument<bool>("-isolated")
                    .action(store_true)
                    .help("if set, remove all isolated vertices");

                auto remove_edge_parser = subparsers.add_parser("-rmedge");

                remove_edge_parser.add_argument<size_t>("ids")
                    .nargs(2)
                    .constraint(valid_zxvertex_id)
                    .metavar("(vs, vt)")
                    .help("the IDs to the two vertices to remove edges in between");

                remove_edge_parser.add_argument<string>("etype")
                    .constraint(choices_allow_prefix({"simple", "hadamard", "all"}))
                    .help("the edge type to remove. Options: simple, hadamard, all (i.e., remove both)");

                auto add_vertex_parser = subparsers.add_parser("-addvertex");

                add_vertex_parser.add_argument<size_t>("qubit")
                    .help("the qubit ID the ZXVertex belongs to");

                add_vertex_parser.add_argument<string>("vtype")
                    .constraint(choices_allow_prefix({"zspider", "xspider", "hbox"}))
                    .help("the type of ZXVertex");

                add_vertex_parser.add_argument<Phase>("phase")
                    .nargs(NArgsOption::optional)
                    .default_value(Phase(0))
                    .help("phase of the ZXVertex (default = 0)");

                auto add_input_parser = subparsers.add_parser("-addinput");

                add_input_parser.add_argument<size_t>("qubit")
                    .constraint(zxgraph_input_qubit_not_exist)
                    .help("the qubit ID of the input");

                auto add_output_parser = subparsers.add_parser("-addoutput");

                add_output_parser.add_argument<size_t>("qubit")
                    .constraint(zxgraph_output_qubit_not_exist)
                    .help("the qubit ID of the output");

                auto add_edge_parser = subparsers.add_parser("-addedge");

                add_edge_parser.add_argument<size_t>("ids")
                    .nargs(2)
                    .constraint(valid_zxvertex_id)
                    .metavar("(vs, vt)")
                    .help("the IDs to the two vertices to add edges in between");

                add_edge_parser.add_argument<string>("etype")
                    .constraint(choices_allow_prefix({"simple", "hadamard"}))
                    .help("the edge type to add. Options: simple, hadamard");
            },
            [](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                if (parser.used_subparser("-rmvertex")) {
                    if (parser.parsed("ids")) {
                        auto ids = parser.get<vector<size_t>>("ids");
                        auto vertices_range = ids |
                                              views::transform([](size_t id) { return ZXGRAPH_MGR.get()->find_vertex_by_id(id); }) |
                                              views::filter([](ZXVertex* v) { return v != nullptr; });
                        for (auto&& v : vertices_range) {
                            LOGGER.info("Removing vertex {}...", v->get_id());
                        }

                        ZXGRAPH_MGR.get()->remove_vertices({vertices_range.begin(), vertices_range.end()});
                    } else if (parser.parsed("-isolated")) {
                        LOGGER.info("Removing isolated vertices...");
                        ZXGRAPH_MGR.get()->remove_isolated_vertices();
                    }
                    return CmdExecResult::done;
                }
                if (parser.used_subparser("-rmedge")) {
                    auto ids = parser.get<std::vector<size_t>>("ids");
                    auto v0 = ZXGRAPH_MGR.get()->find_vertex_by_id(ids[0]);
                    auto v1 = ZXGRAPH_MGR.get()->find_vertex_by_id(ids[1]);
                    assert(v0 != nullptr && v1 != nullptr);

                    auto etype = str_to_edge_type(parser.get<std::string>("etype"));

                    if (etype.has_value()) {
                        LOGGER.info("Removing edge ({}, {}), edge type: {}...", v0->get_id(), v1->get_id(), etype.value());
                        ZXGRAPH_MGR.get()->remove_edge(v0, v1, etype.value());
                    } else {
                        LOGGER.info("Removing all edges between ({}, {})...", v0->get_id(), v1->get_id());
                        ZXGRAPH_MGR.get()->remove_all_edges_between(v0, v1);
                    }

                    return CmdExecResult::done;
                }
                if (parser.used_subparser("-addvertex")) {
                    auto vtype = str_to_vertex_type(parser.get<std::string>("vtype"));
                    assert(vtype.has_value());

                    auto v = ZXGRAPH_MGR.get()->add_vertex(parser.get<size_t>("qubit"), vtype.value(), parser.get<Phase>("phase"));
                    LOGGER.info("Adding vertex {}...", v->get_id());
                    return CmdExecResult::done;
                }
                if (parser.used_subparser("-addinput")) {
                    auto i = ZXGRAPH_MGR.get()->add_input(parser.get<size_t>("qubit"));
                    LOGGER.info("Adding input {}...", i->get_id());
                    return CmdExecResult::done;
                }
                if (parser.used_subparser("-addoutput")) {
                    auto o = ZXGRAPH_MGR.get()->add_output(parser.get<size_t>("qubit"));
                    LOGGER.info("Adding output {}...", o->get_id());
                    return CmdExecResult::done;
                }
                if (parser.used_subparser("-addedge")) {
                    auto ids = parser.get<std::vector<size_t>>("ids");
                    auto vs = ZXGRAPH_MGR.get()->find_vertex_by_id(ids[0]);
                    auto vt = ZXGRAPH_MGR.get()->find_vertex_by_id(ids[1]);
                    assert(vs != nullptr && vt != nullptr);

                    auto etype = str_to_edge_type(parser.get<std::string>("etype"));
                    assert(etype.has_value());

                    if (vs->is_neighbor(vt, etype.value()) && (vs->is_boundary() || vt->is_boundary())) {
                        LOGGER.fatal("Cannot add edge between boundary vertices {} and {}", vs->get_id(), vt->get_id());
                        return CmdExecResult::error;
                    }

                    bool had_edge = vs->is_neighbor(vt, etype.value());

                    ZXGRAPH_MGR.get()->add_edge(vs, vt, etype.value());

                    if (vs == vt) {
                        LOGGER.info("Note: converting this self-loop to phase {} on vertex {}...", etype.value() == EdgeType::hadamard ? Phase(1) : Phase(0), vs->get_id());
                    } else if (had_edge) {
                        bool has_edge = vs->is_neighbor(vt, etype.value());
                        if (has_edge) {
                            LOGGER.info("Note: redundant edge; merging into existing edge ({}, {})...", vs->get_id(), vt->get_id());
                        } else {
                            LOGGER.info("Note: Hopf edge; cancelling out with existing edge ({}, {})...", vs->get_id(), vt->get_id());
                        }
                    } else {
                        LOGGER.info("Adding edge ({}, {}), edge type: {}...", vs->get_id(), vt->get_id(), etype.value());
                    }

                    return CmdExecResult::done;
                }
                return CmdExecResult::error;
            }};
}

//----------------------------------------------------------------------
//    ZXGTRaverse
//----------------------------------------------------------------------
Command zxgraph_traverse_cmd() {
    return {"zxgtraverse",
            [](ArgumentParser& parser) {
                parser.description("traverse ZXGraph and update topological order of vertices");
            },
            [](ArgumentParser const& /*parser*/) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                ZXGRAPH_MGR.get()->update_topological_order();
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXGDraw [-CLI]
//    ZXGDraw <string (path.pdf)>
//----------------------------------------------------------------------

Command zxgraph_draw_cmd() {
    return {"zxgdraw",
            [](ArgumentParser& parser) {
                parser.description("draw ZXGraph");

                parser.add_argument<string>("filepath")
                    .nargs(NArgsOption::optional)
                    .constraint(path_writable)
                    .constraint(allowed_extension({".pdf"}))
                    .help("the output path. Supported extension: .pdf");

                parser.add_argument<bool>("-cli")
                    .action(store_true)
                    .help("print to the terminal. Note that only horizontal wires will be printed");
            },
            [](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                if (parser.parsed("filepath")) {
                    if (!ZXGRAPH_MGR.get()->write_pdf(parser.get<string>("filepath"))) return CmdExecResult::error;
                }
                if (parser.parsed("-cli")) {
                    ZXGRAPH_MGR.get()->draw();
                }

                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZX2TS
//----------------------------------------------------------------------
Command zxgraph_to_tensor_cmd() {
    return {"zx2ts",
            [](ArgumentParser& parser) {
                parser.description("convert ZXGraph to tensor");

                parser.add_argument<size_t>("-zx")
                    .metavar("id")
                    .constraint(valid_zxgraph_id)
                    .help("the ID of the ZXGraph to be converted. If not specified, the focused ZXGraph is used");

                parser.add_argument<size_t>("-ts")
                    .metavar("id")
                    .help("the ID of the target tensor. If not specified, an ID is automatically assigned");

                parser.add_argument<bool>("-replace")
                    .action(store_true)
                    .help("replace the target tensor if the tensor ID is occupied");
            },
            [](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                auto zx_id = parser.parsed("-zx") ? parser.get<size_t>("-zx") : ZXGRAPH_MGR.focused_id();
                auto zx = ZXGRAPH_MGR.find_by_id(zx_id);

                auto ts_id = parser.parsed("-ts") ? parser.get<size_t>("-ts") : TENSOR_MGR.get_next_id();

                if (TENSOR_MGR.is_id(ts_id) && !parser.parsed("-replace")) {
                    LOGGER.error("Tensor {} already exists!! Specify `-Replace` if you intend to replace the current one.", ts_id);
                    return CmdExecResult::error;
                }
                LOGGER.info("Converting ZXGraph {} to Tensor {}...", zx_id, ts_id);
                auto tensor = to_tensor(*zx);

                if (tensor.has_value()) {
                    if (!TENSOR_MGR.is_id(ts_id)) {
                        TENSOR_MGR.add(ts_id, std::make_unique<QTensor<double>>(std::move(tensor.value())));
                    } else {
                        TENSOR_MGR.checkout(ts_id);
                        TENSOR_MGR.set(std::make_unique<QTensor<double>>(std::move(tensor.value())));
                    }

                    TENSOR_MGR.get()->set_filename(zx->get_filename());
                    TENSOR_MGR.get()->add_procedures(zx->get_procedures());
                    TENSOR_MGR.get()->add_procedure("ZX2TS");
                }

                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXGRead <string Input.(b)zx> [-KEEPid] [-Replace]
//----------------------------------------------------------------------

Command zxgraph_read_cmd() {
    return {"zxgread",
            [](ArgumentParser& parser) {
                parser.description("read a file and construct the corresponding ZXGraph");

                parser.add_argument<string>("filepath")
                    .constraint(path_readable)
                    .constraint(allowed_extension({".zx", ".bzx"}))
                    .help("path to the ZX file. Supported extensions: .zx, .bzx");

                parser.add_argument<bool>("-keepid")
                    .action(store_true)
                    .help("if set, retain the IDs in the ZX file; otherwise the ID is rearranged to be consecutive");

                parser.add_argument<bool>("-replace")
                    .action(store_true)
                    .constraint(zxgraph_id_not_exist)
                    .help("replace the current ZXGraph");
            },
            [](ArgumentParser const& parser) {
                auto filepath = parser.get<string>("filepath");
                auto do_keep_id = parser.get<bool>("-keepid");
                auto do_replace = parser.get<bool>("-replace");

                auto buffer_graph = make_unique<ZXGraph>();
                if (!buffer_graph->read_zx(filepath, do_keep_id)) {
                    return CmdExecResult::error;
                }

                if (do_replace) {
                    if (ZXGRAPH_MGR.empty()) {
                        cout << "Note: ZXGraph list is empty now. Create a new one." << endl;
                        ZXGRAPH_MGR.add(ZXGRAPH_MGR.get_next_id());
                    } else {
                        cout << "Note: original ZXGraph is replaced..." << endl;
                    }
                    ZXGRAPH_MGR.set(std::move(buffer_graph));
                } else {
                    ZXGRAPH_MGR.add(ZXGRAPH_MGR.get_next_id(), std::move(buffer_graph));
                }
                ZXGRAPH_MGR.get()->set_filename(std::filesystem::path{filepath}.stem());
                return CmdExecResult::done;
            }};
}

Command zxgraph_write_cmd() {
    return {"zxgwrite",
            [](ArgumentParser& parser) {
                parser.description("write the ZXGraph to a file");

                parser.add_argument<string>("filepath")
                    .constraint(path_writable)
                    .constraint(allowed_extension({".zx", ".bzx", ".tikz", ".tex", ""}))
                    .help("the path to the output ZX file");

                parser.add_argument<bool>("-complete")
                    .action(store_true)
                    .help("if specified, output neighbor information on both vertices of each edge");
            },
            [](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                auto filepath = parser.get<string>("filepath");
                auto do_complete = parser.get<bool>("-complete");
                size_t extension_pos = filepath.find_last_of('.');

                string extension = (extension_pos == string::npos) ? "" : filepath.substr(extension_pos);
                if (extension == ".zx" || extension == ".bzx" || extension == "") {
                    if (!ZXGRAPH_MGR.get()->write_zx(filepath, do_complete)) {
                        cerr << "Error: fail to write ZXGraph to \"" << filepath << "\"!!\n";
                        return CmdExecResult::error;
                    }
                } else if (extension == ".tikz") {
                    if (!ZXGRAPH_MGR.get()->write_tikz(filepath)) {
                        cerr << "Error: fail to write Tikz to \"" << filepath << "\"!!\n";
                        return CmdExecResult::error;
                    }
                } else if (extension == ".tex") {
                    if (!ZXGRAPH_MGR.get()->write_tex(filepath)) {
                        cerr << "Error: fail to write tex to \"" << filepath << "\"!!\n";
                        return CmdExecResult::error;
                    }
                }
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXGASsign <size_t qubit> <I|O> <VertexType vt> <string Phase>
//----------------------------------------------------------------------

Command zxgraph_assign_boundary_cmd() {
    return {"zxgassign",
            [](ArgumentParser& parser) {
                parser.description("assign quantum states to input/output vertex");

                parser.add_argument<size_t>("qubit")
                    .help("the qubit to assign state to");

                parser.add_argument<string>("io")
                    .constraint(choices_allow_prefix({"input", "output"}))
                    .metavar("input/output")
                    .help("add at input or output");

                parser.add_argument<string>("vtype")
                    .constraint(choices_allow_prefix({"zspider", "xspider", "hbox"}))
                    .help("the type of ZXVertex");

                parser.add_argument<Phase>("phase")
                    .help("the phase of the vertex");
            },
            [](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                auto qid = parser.get<size_t>("qubit");
                bool is_input = dvlab::str::to_lower_string(parser.get<string>("io")).starts_with('i');

                if (!(is_input ? ZXGRAPH_MGR.get()->is_input_qubit(qid) : ZXGRAPH_MGR.get()->is_output_qubit(qid))) {
                    cerr << "Error: the specified boundary does not exist!!" << endl;
                    return CmdExecResult::error;
                }

                auto vtype = str_to_vertex_type(parser.get<std::string>("vtype"));
                assert(vtype.has_value());

                auto phase = parser.get<Phase>("phase");
                ZXGRAPH_MGR.get()->assign_vertex_to_boundary(qid, is_input, vtype.value(), phase);

                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXGADJoint
//----------------------------------------------------------------------
Command zxgraph_adjoint_cmd() {
    return {"zxgadjoint",
            [](ArgumentParser& parser) {
                parser.description("adjoint ZXGraph");
            },
            [](ArgumentParser const& /*parser*/) {
                if (!zxgraph_mgr_not_empty()) return CmdExecResult::error;
                ZXGRAPH_MGR.get()->adjoint();
                return CmdExecResult::done;
            }};
}
