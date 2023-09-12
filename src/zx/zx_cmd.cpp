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
#include <functional>
#include <string>

#include "./zxgraph_mgr.hpp"
#include "tensor/tensor_mgr.hpp"
#include "zx/zx_def.hpp"
#include "zx/zxgraph.hpp"

using namespace dvlab::argparse;
using dvlab::CmdExecResult;
using dvlab::Command;

namespace qsyn::zx {

std::function<bool(size_t const&)> valid_zxgraph_id(ZXGraphMgr const& zxgraph_mgr) {
    return [&](size_t const& id) {
        if (zxgraph_mgr.is_id(id)) return true;
        LOGGER.error("ZXGraph {} does not exist!!", id);
        return false;
    };
}

std::function<bool(size_t const&)> valid_zxvertex_id(ZXGraphMgr const& zxgraph_mgr) {
    return [&](size_t const& id) {
        if (zxgraph_mgr.get()->is_v_id(id)) return true;
        LOGGER.error("Cannot find vertex with ID {} in the ZXGraph!!", id);
        return false;
    };
}

std::function<bool(size_t const&)> zxgraph_id_not_exist(ZXGraphMgr const& zxgraph_mgr) {
    return [&](size_t const& id) {
        if (!zxgraph_mgr.is_id(id)) return true;
        LOGGER.error("ZXGraph {} already exists!!", id);
        LOGGER.info("Use `-Replace` if you want to overwrite it.");
        return false;
    };
}

std::function<bool(size_t const&)> zxgraph_input_qubit_not_exist(ZXGraphMgr const& zxgraph_mgr) {
    return [&](size_t const& qid) {
        if (!zxgraph_mgr.get()->is_input_qubit(qid)) return true;
        LOGGER.error("This qubit's input already exists!!");
        return false;
    };
}

std::function<bool(size_t const&)> zxgraph_output_qubit_not_exist(ZXGraphMgr const& zxgraph_mgr) {
    return [&](size_t const& qid) {
        if (!zxgraph_mgr.get()->is_output_qubit(qid)) return true;
        LOGGER.error("This qubit's output already exists!!");
        return false;
    };
}

bool zxgraph_mgr_not_empty(ZXGraphMgr const& zxgraph_mgr) {
    if (zxgraph_mgr.empty()) {
        LOGGER.error("ZXGraph list is empty. Please create a ZXGraph first!!");
        LOGGER.info("Use ZXNew to add a new ZXGraph, or ZXGRead to read a ZXGraph from a file.");
        return false;
    }
    return true;
}

//----------------------------------------------------------------------
//    ZXCHeckout <(size_t id)>
//----------------------------------------------------------------------
Command zxgraph_checkout_cmd(ZXGraphMgr& zxgraph_mgr) {
    return {"zxcheckout",
            [&](ArgumentParser& parser) {
                parser.description("checkout to Graph <id> in ZXGraphMgr");
                parser.add_argument<size_t>("id")
                    .constraint(valid_zxgraph_id(zxgraph_mgr))
                    .help("the ID of the ZXGraph");
            },
            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                zxgraph_mgr.checkout(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXNew [(size_t id)]
//----------------------------------------------------------------------
Command zxgraph_new_cmd(ZXGraphMgr& zxgraph_mgr) {
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
            [&](ArgumentParser const& parser) {
                size_t id = (parser.parsed("id")) ? parser.get<size_t>("id") : zxgraph_mgr.get_next_id();

                if (zxgraph_mgr.is_id(id)) {
                    if (!parser.parsed("-Replace")) {
                        LOGGER.error("ZXGraph {} already exists!! Specify `-Replace` if needed.", id);
                        return CmdExecResult::error;
                    }
                    zxgraph_mgr.set(std::make_unique<ZXGraph>());
                    return CmdExecResult::done;
                } else {
                    zxgraph_mgr.add(id);
                }

                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXReset
//----------------------------------------------------------------------
Command zxgraph_mgr_reset_cmd(ZXGraphMgr& zxgraph_mgr) {
    return {"zxreset",
            [](ArgumentParser& parser) {
                parser.description("reset ZXGraphMgr");
            },
            [&](ArgumentParser const& /*parser*/) {
                zxgraph_mgr.reset();
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXDelete <(size_t id)>
//----------------------------------------------------------------------
Command zxgraph_delete_cmd(ZXGraphMgr& zxgraph_mgr) {
    return {"zxdelete",
            [&](ArgumentParser& parser) {
                parser.description("remove a ZXGraph from ZXGraphMgr");

                parser.add_argument<size_t>("id")
                    .constraint(valid_zxgraph_id(zxgraph_mgr))
                    .help("the ID of the ZXGraph");
            },
            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                zxgraph_mgr.remove(parser.get<size_t>("id"));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXPrint [-Summary | -Focus | -Num]
//----------------------------------------------------------------------
Command zxgraph_mgr_print_cmd(ZXGraphMgr const& zxgraph_mgr) {
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
            [&](ArgumentParser const& parser) {
                if (parser.parsed("-focus"))
                    zxgraph_mgr.print_focus();
                else if (parser.parsed("-list"))
                    zxgraph_mgr.print_list();
                else
                    zxgraph_mgr.print_manager();
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXCOPy [(size_t id)]
//----------------------------------------------------------------------
Command zxgraph_copy_cmd(ZXGraphMgr& zxgraph_mgr) {
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
            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                size_t id = (parser.parsed("id")) ? parser.get<size_t>("id") : zxgraph_mgr.get_next_id();
                if (zxgraph_mgr.is_id(id)) {
                    if (!parser.parsed("-replace")) {
                        LOGGER.error("ZXGraph {} already exists!! Specify `-Replace` if needed.", id);
                        return CmdExecResult::error;
                    }
                    zxgraph_mgr.copy(id);
                    return CmdExecResult::done;
                }

                zxgraph_mgr.copy(id);
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXCOMpose <size_t id>
//----------------------------------------------------------------------
Command zxgraph_compose_cmd(ZXGraphMgr& zxgraph_mgr) {
    return {"zxcompose",
            [&](ArgumentParser& parser) {
                parser.description("compose a ZXGraph");

                parser.add_argument<size_t>("id")
                    .constraint(valid_zxgraph_id(zxgraph_mgr))
                    .help("the ID of the ZXGraph to compose with");
            },
            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                zxgraph_mgr.get()->compose(*zxgraph_mgr.find_by_id(parser.get<size_t>("id")));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXTensor <size_t id>
//----------------------------------------------------------------------
Command zxgraph_tensor_product_cmd(ZXGraphMgr& zxgraph_mgr) {
    return {"zxtensor",
            [&](ArgumentParser& parser) {
                parser.description("tensor a ZXGraph");

                parser.add_argument<size_t>("id")
                    .constraint(valid_zxgraph_id(zxgraph_mgr))
                    .help("the ID of the ZXGraph");
            },
            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                zxgraph_mgr.get()->tensor_product(*zxgraph_mgr.find_by_id(parser.get<size_t>("id")));
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXGTest [-Empty | -Valid | -GLike | -IDentity]
//----------------------------------------------------------------------
Command zxgraph_test_cmd(ZXGraphMgr const& zxgraph_mgr) {
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
            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                if (parser.parsed("-empty")) {
                    if (zxgraph_mgr.get()->is_empty())
                        std::cout << "The graph is empty!" << std::endl;
                    else
                        std::cout << "The graph is not empty!" << std::endl;
                } else if (parser.parsed("-valid")) {
                    if (zxgraph_mgr.get()->is_valid())
                        std::cout << "The graph is valid!" << std::endl;
                    else
                        std::cout << "The graph is invalid!" << std::endl;
                } else if (parser.parsed("-glike")) {
                    if (zxgraph_mgr.get()->is_graph_like())
                        std::cout << "The graph is graph-like!" << std::endl;
                    else
                        std::cout << "The graph is not graph-like!" << std::endl;
                } else if (parser.parsed("-identity")) {
                    if (zxgraph_mgr.get()->is_identity())
                        std::cout << "The graph is an identity!" << std::endl;
                    else
                        std::cout << "The graph is not an identity!" << std::endl;
                }
                return CmdExecResult::done;
            }};
}

//-----------------------------------------------------------------------------------------------------------
//    ZXGPrint [-Summary | -Inputs | -Outputs | -Vertices | -Edges | -Qubits | -Neighbors | -Analysis | -Density]
//-----------------------------------------------------------------------------------------------------------
Command zxgraph_print_cmd(ZXGraphMgr const& zxgraph_mgr) {
    return {"zxgprint",
            [&](ArgumentParser& parser) {
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
                    .constraint(valid_zxvertex_id(zxgraph_mgr))
                    .help("print the vertex info of ZXGraph");
                mutex.add_argument<bool>("-edges")
                    .action(store_true)
                    .help("print the edges info of ZXGraph");
                mutex.add_argument<int>("-qubits")
                    .nargs(NArgsOption::zero_or_more)
                    .help("print the qubit info of ZXGraph");
                mutex.add_argument<size_t>("-neighbors")
                    .constraint(valid_zxvertex_id(zxgraph_mgr))
                    .help("print the neighbor info of ZXGraph");
                mutex.add_argument<bool>("-density")
                    .action(store_true)
                    .help("print the density of ZXGraph");
            },

            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                if (parser.parsed("-summary")) {
                    zxgraph_mgr.get()->print_graph();
                    fmt::println("{:<29} {}", "#T-gate:", zxgraph_mgr.get()->t_count());
                    fmt::println("{:<29} {}", "#Non-(Clifford+T)-gate: ", zxgraph_mgr.get()->non_clifford_t_count());
                    fmt::println("{:<29} {}", "#Non-Clifford-gate: ", zxgraph_mgr.get()->non_clifford_count());
                } else if (parser.parsed("-io"))
                    zxgraph_mgr.get()->print_io();
                else if (parser.parsed("-list"))
                    zxgraph_mgr.print_list();
                else if (parser.parsed("-inputs"))
                    zxgraph_mgr.get()->print_inputs();
                else if (parser.parsed("-outputs"))
                    zxgraph_mgr.get()->print_outputs();
                else if (parser.parsed("-vertices")) {
                    auto vids = parser.get<std::vector<size_t>>("-vertices");
                    if (vids.empty())
                        zxgraph_mgr.get()->print_vertices();
                    else
                        zxgraph_mgr.get()->print_vertices(vids);
                } else if (parser.parsed("-edges"))
                    zxgraph_mgr.get()->print_edges();
                else if (parser.parsed("-qubits")) {
                    auto qids = parser.get<std::vector<int>>("-qubits");
                    zxgraph_mgr.get()->print_qubits(qids);
                } else if (parser.parsed("-neighbors")) {
                    auto v = zxgraph_mgr.get()->find_vertex_by_id(parser.get<size_t>("-neighbors"));
                    v->print_vertex();
                    std::cout << "----- Neighbors -----" << std::endl;
                    for (auto [nb, _] : v->get_neighbors()) {
                        nb->print_vertex();
                    }
                } else if (parser.parsed("-density")) {
                    std::cout << "Density: " << zxgraph_mgr.get()->density() << std::endl;
                } else
                    zxgraph_mgr.get()->print_graph();
                return CmdExecResult::done;
            }};
}

Command zxgraph_edit_cmd(ZXGraphMgr& zxgraph_mgr) {
    return {"zxgedit",
            [&](ArgumentParser& parser) {
                parser.description("edit ZXGraph");

                auto subparsers = parser.add_subparsers().required(true);

                auto remove_vertex_parser = subparsers.add_parser("-rmvertex");

                auto rmv_mutex = remove_vertex_parser.add_mutually_exclusive_group().required(true);

                rmv_mutex.add_argument<size_t>("ids")
                    .constraint(valid_zxvertex_id(zxgraph_mgr))
                    .nargs(NArgsOption::zero_or_more)
                    .help("the IDs of vertices to remove");

                rmv_mutex.add_argument<bool>("-isolated")
                    .action(store_true)
                    .help("if set, remove all isolated vertices");

                auto remove_edge_parser = subparsers.add_parser("-rmedge");

                remove_edge_parser.add_argument<size_t>("ids")
                    .nargs(2)
                    .constraint(valid_zxvertex_id(zxgraph_mgr))
                    .metavar("(vs, vt)")
                    .help("the IDs to the two vertices to remove edges in between");

                remove_edge_parser.add_argument<std::string>("etype")
                    .constraint(choices_allow_prefix({"simple", "hadamard", "all"}))
                    .help("the edge type to remove. Options: simple, hadamard, all (i.e., remove both)");

                auto add_vertex_parser = subparsers.add_parser("-addvertex");

                add_vertex_parser.add_argument<size_t>("qubit")
                    .help("the qubit ID the ZXVertex belongs to");

                add_vertex_parser.add_argument<std::string>("vtype")
                    .constraint(choices_allow_prefix({"zspider", "xspider", "hbox"}))
                    .help("the type of ZXVertex");

                add_vertex_parser.add_argument<Phase>("phase")
                    .nargs(NArgsOption::optional)
                    .default_value(Phase(0))
                    .help("phase of the ZXVertex (default = 0)");

                auto add_input_parser = subparsers.add_parser("-addinput");

                add_input_parser.add_argument<size_t>("qubit")
                    .constraint(zxgraph_input_qubit_not_exist(zxgraph_mgr))
                    .help("the qubit ID of the input");

                auto add_output_parser = subparsers.add_parser("-addoutput");

                add_output_parser.add_argument<size_t>("qubit")
                    .constraint(zxgraph_output_qubit_not_exist(zxgraph_mgr))
                    .help("the qubit ID of the output");

                auto add_edge_parser = subparsers.add_parser("-addedge");

                add_edge_parser.add_argument<size_t>("ids")
                    .nargs(2)
                    .constraint(valid_zxvertex_id(zxgraph_mgr))
                    .metavar("(vs, vt)")
                    .help("the IDs to the two vertices to add edges in between");

                add_edge_parser.add_argument<std::string>("etype")
                    .constraint(choices_allow_prefix({"simple", "hadamard"}))
                    .help("the edge type to add. Options: simple, hadamard");
            },
            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                if (parser.used_subparser("-rmvertex")) {
                    if (parser.parsed("ids")) {
                        auto ids = parser.get<std::vector<size_t>>("ids");
                        auto vertices_range = ids |
                                              std::views::transform([&](size_t id) { return zxgraph_mgr.get()->find_vertex_by_id(id); }) |
                                              std::views::filter([](ZXVertex* v) { return v != nullptr; });
                        for (auto&& v : vertices_range) {
                            LOGGER.info("Removing vertex {}...", v->get_id());
                        }

                        zxgraph_mgr.get()->remove_vertices({vertices_range.begin(), vertices_range.end()});
                    } else if (parser.parsed("-isolated")) {
                        LOGGER.info("Removing isolated vertices...");
                        zxgraph_mgr.get()->remove_isolated_vertices();
                    }
                    return CmdExecResult::done;
                }
                if (parser.used_subparser("-rmedge")) {
                    auto ids = parser.get<std::vector<size_t>>("ids");
                    auto v0 = zxgraph_mgr.get()->find_vertex_by_id(ids[0]);
                    auto v1 = zxgraph_mgr.get()->find_vertex_by_id(ids[1]);
                    assert(v0 != nullptr && v1 != nullptr);

                    auto etype = str_to_edge_type(parser.get<std::string>("etype"));

                    if (etype.has_value()) {
                        LOGGER.info("Removing edge ({}, {}), edge type: {}...", v0->get_id(), v1->get_id(), etype.value());
                        zxgraph_mgr.get()->remove_edge(v0, v1, etype.value());
                    } else {
                        LOGGER.info("Removing all edges between ({}, {})...", v0->get_id(), v1->get_id());
                        zxgraph_mgr.get()->remove_all_edges_between(v0, v1);
                    }

                    return CmdExecResult::done;
                }
                if (parser.used_subparser("-addvertex")) {
                    auto vtype = str_to_vertex_type(parser.get<std::string>("vtype"));
                    assert(vtype.has_value());

                    auto v = zxgraph_mgr.get()->add_vertex(parser.get<size_t>("qubit"), vtype.value(), parser.get<Phase>("phase"));
                    LOGGER.info("Adding vertex {}...", v->get_id());
                    return CmdExecResult::done;
                }
                if (parser.used_subparser("-addinput")) {
                    auto i = zxgraph_mgr.get()->add_input(parser.get<size_t>("qubit"));
                    LOGGER.info("Adding input {}...", i->get_id());
                    return CmdExecResult::done;
                }
                if (parser.used_subparser("-addoutput")) {
                    auto o = zxgraph_mgr.get()->add_output(parser.get<size_t>("qubit"));
                    LOGGER.info("Adding output {}...", o->get_id());
                    return CmdExecResult::done;
                }
                if (parser.used_subparser("-addedge")) {
                    auto ids = parser.get<std::vector<size_t>>("ids");
                    auto vs = zxgraph_mgr.get()->find_vertex_by_id(ids[0]);
                    auto vt = zxgraph_mgr.get()->find_vertex_by_id(ids[1]);
                    assert(vs != nullptr && vt != nullptr);

                    auto etype = str_to_edge_type(parser.get<std::string>("etype"));
                    assert(etype.has_value());

                    if (vs->is_neighbor(vt, etype.value()) && (vs->is_boundary() || vt->is_boundary())) {
                        LOGGER.fatal("Cannot add edge between boundary vertices {} and {}", vs->get_id(), vt->get_id());
                        return CmdExecResult::error;
                    }

                    bool had_edge = vs->is_neighbor(vt, etype.value());

                    zxgraph_mgr.get()->add_edge(vs, vt, etype.value());

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
Command zxgraph_traverse_cmd(ZXGraphMgr& zxgraph_mgr) {
    return {"zxgtraverse",
            [](ArgumentParser& parser) {
                parser.description("traverse ZXGraph and update topological order of vertices");
            },
            [&](ArgumentParser const& /*parser*/) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                zxgraph_mgr.get()->update_topological_order();
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXGDraw [-CLI]
//    ZXGDraw <string (path.pdf)>
//----------------------------------------------------------------------

Command zxgraph_draw_cmd(ZXGraphMgr const& zxgraph_mgr) {
    return {"zxgdraw",
            [](ArgumentParser& parser) {
                parser.description("draw ZXGraph");

                parser.add_argument<std::string>("filepath")
                    .nargs(NArgsOption::optional)
                    .constraint(path_writable)
                    .constraint(allowed_extension({".pdf"}))
                    .help("the output path. Supported extension: .pdf");

                parser.add_argument<bool>("-cli")
                    .action(store_true)
                    .help("print to the terminal. Note that only horizontal wires will be printed");
            },
            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                if (parser.parsed("filepath")) {
                    if (!zxgraph_mgr.get()->write_pdf(parser.get<std::string>("filepath"))) return CmdExecResult::error;
                }
                if (parser.parsed("-cli")) {
                    zxgraph_mgr.get()->draw();
                }

                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXGRead <string Input.(b)zx> [-KEEPid] [-Replace]
//----------------------------------------------------------------------

Command zxgraph_read_cmd(ZXGraphMgr& zxgraph_mgr) {
    return {"zxgread",
            [&](ArgumentParser& parser) {
                parser.description("read a file and construct the corresponding ZXGraph");

                parser.add_argument<std::string>("filepath")
                    .constraint(path_readable)
                    .constraint(allowed_extension({".zx", ".bzx"}))
                    .help("path to the ZX file. Supported extensions: .zx, .bzx");

                parser.add_argument<bool>("-keepid")
                    .action(store_true)
                    .help("if set, retain the IDs in the ZX file; otherwise the ID is rearranged to be consecutive");

                parser.add_argument<bool>("-replace")
                    .action(store_true)
                    .constraint(zxgraph_id_not_exist(zxgraph_mgr))
                    .help("replace the current ZXGraph");
            },
            [&](ArgumentParser const& parser) {
                auto filepath = parser.get<std::string>("filepath");
                auto do_keep_id = parser.get<bool>("-keepid");
                auto do_replace = parser.get<bool>("-replace");

                auto buffer_graph = std::make_unique<ZXGraph>();
                if (!buffer_graph->read_zx(filepath, do_keep_id)) {
                    return CmdExecResult::error;
                }

                if (do_replace) {
                    if (zxgraph_mgr.empty()) {
                        std::cout << "Note: ZXGraph list is empty now. Create a new one." << std::endl;
                        zxgraph_mgr.add(zxgraph_mgr.get_next_id());
                    } else {
                        std::cout << "Note: original ZXGraph is replaced..." << std::endl;
                    }
                    zxgraph_mgr.set(std::move(buffer_graph));
                } else {
                    zxgraph_mgr.add(zxgraph_mgr.get_next_id(), std::move(buffer_graph));
                }
                zxgraph_mgr.get()->set_filename(std::filesystem::path{filepath}.stem());
                return CmdExecResult::done;
            }};
}

Command zxgraph_write_cmd(ZXGraphMgr const& zxgraph_mgr) {
    return {"zxgwrite",
            [](ArgumentParser& parser) {
                parser.description("write the ZXGraph to a file");

                parser.add_argument<std::string>("filepath")
                    .constraint(path_writable)
                    .constraint(allowed_extension({".zx", ".bzx", ".tikz", ".tex", ""}))
                    .help("the path to the output ZX file");

                parser.add_argument<bool>("-complete")
                    .action(store_true)
                    .help("if specified, output neighbor information on both vertices of each edge");
            },
            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                auto filepath = parser.get<std::string>("filepath");
                auto do_complete = parser.get<bool>("-complete");
                size_t extension_pos = filepath.find_last_of('.');

                std::string extension = (extension_pos == std::string::npos) ? "" : filepath.substr(extension_pos);
                if (extension == ".zx" || extension == ".bzx" || extension == "") {
                    if (!zxgraph_mgr.get()->write_zx(filepath, do_complete)) {
                        std::cerr << "Error: fail to write ZXGraph to \"" << filepath << "\"!!\n";
                        return CmdExecResult::error;
                    }
                } else if (extension == ".tikz") {
                    if (!zxgraph_mgr.get()->write_tikz(filepath)) {
                        std::cerr << "Error: fail to write Tikz to \"" << filepath << "\"!!\n";
                        return CmdExecResult::error;
                    }
                } else if (extension == ".tex") {
                    if (!zxgraph_mgr.get()->write_tex(filepath)) {
                        std::cerr << "Error: fail to write tex to \"" << filepath << "\"!!\n";
                        return CmdExecResult::error;
                    }
                }
                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXGASsign <size_t qubit> <I|O> <VertexType vt> <string Phase>
//----------------------------------------------------------------------

Command zxgraph_assign_boundary_cmd(ZXGraphMgr& zxgraph_mgr) {
    return {"zxgassign",
            [](ArgumentParser& parser) {
                parser.description("assign quantum states to input/output vertex");

                parser.add_argument<size_t>("qubit")
                    .help("the qubit to assign state to");

                parser.add_argument<std::string>("io")
                    .constraint(choices_allow_prefix({"input", "output"}))
                    .metavar("input/output")
                    .help("add at input or output");

                parser.add_argument<std::string>("vtype")
                    .constraint(choices_allow_prefix({"zspider", "xspider", "hbox"}))
                    .help("the type of ZXVertex");

                parser.add_argument<Phase>("phase")
                    .help("the phase of the vertex");
            },
            [&](ArgumentParser const& parser) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                auto qid = parser.get<size_t>("qubit");
                bool is_input = dvlab::str::tolower_string(parser.get<std::string>("io")).starts_with('i');

                if (!(is_input ? zxgraph_mgr.get()->is_input_qubit(qid) : zxgraph_mgr.get()->is_output_qubit(qid))) {
                    std::cerr << "Error: the specified boundary does not exist!!" << std::endl;
                    return CmdExecResult::error;
                }

                auto vtype = str_to_vertex_type(parser.get<std::string>("vtype"));
                assert(vtype.has_value());

                auto phase = parser.get<Phase>("phase");
                zxgraph_mgr.get()->assign_vertex_to_boundary(qid, is_input, vtype.value(), phase);

                return CmdExecResult::done;
            }};
}

//----------------------------------------------------------------------
//    ZXGADJoint
//----------------------------------------------------------------------
Command zxgraph_adjoint_cmd(ZXGraphMgr& zxgraph_mgr) {
    return {"zxgadjoint",
            [](ArgumentParser& parser) {
                parser.description("adjoint ZXGraph");
            },
            [&](ArgumentParser const& /*parser*/) {
                if (!zxgraph_mgr_not_empty(zxgraph_mgr)) return CmdExecResult::error;
                zxgraph_mgr.get()->adjoint();
                return CmdExecResult::done;
            }};
}

bool add_zx_cmds(dvlab::CommandLineInterface& cli, ZXGraphMgr& zxgraph_mgr) {
    if (!(cli.add_command(zxgraph_checkout_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_new_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_mgr_reset_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_delete_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_copy_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_compose_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_tensor_product_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_mgr_print_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_print_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_test_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_edit_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_adjoint_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_assign_boundary_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_traverse_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_draw_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_read_cmd(zxgraph_mgr)) &&
          cli.add_command(zxgraph_write_cmd(zxgraph_mgr)))) {
        std::cerr << "Registering \"zx\" commands fails... exiting" << std::endl;
        return false;
    }
    return true;
}

}  // namespace qsyn::zx