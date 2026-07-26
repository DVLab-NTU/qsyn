#include "cmd/ncf_mgr.hpp"

#include <fstream>

#include "argparse/arg_parser.hpp"
#include "cmd/qcir_mgr.hpp"
#include "cmd/tableau_mgr.hpp"
#include "ncf/ncf_analysis.hpp"
#include "ncf/ncf_convert.hpp"
#include "ncf/ncf_program.hpp"
#include "util/data_structure_manager_common_cmd.hpp"

using namespace dvlab::argparse;

namespace qsyn::experimental {

namespace {

void print_block_pauli(NcfBlock const& b) {
    fmt::println("Block {} pivot={}", b.id(), b.pivot() ? fmt::format("{}", *b.pivot()) : "none");
    fmt::println("  original indices: {}", fmt::join(b.original_indices(), ", "));
    fmt::println("  before:");
    for (auto const& p : b.pauli_before()) fmt::println("    {:c}", p);
    fmt::println("  after:");
    for (auto const& p : b.pauli_after()) fmt::println("    {:c}", p);
}

void print_block_stats(NcfBlock const& b) {
    auto s = b.stats();
    fmt::println("Block {}: cx_prefix={} cx_suffix={} cx_total={} inner_rz={} hs={}",
                 b.id(), s.cx_prefix, s.cx_suffix, s.cx_total, s.inner_rz_count, s.hs_count);
}

void print_clifford_layer(char const* label, CliffordOperatorString const& ops) {
    fmt::println("{} CX:", label);
    for (size_t i = 0; i < ops.size(); ++i) {
        auto const& [type, qubits] = ops[i];
        if (type == CliffordOperatorType::cx) {
            fmt::println("  [{}] cx({}, {})", i, qubits[0], qubits[1]);
        }
    }
    fmt::println("{} H/S:", label);
    for (size_t i = 0; i < ops.size(); ++i) {
        auto const& [type, qubits] = ops[i];
        if (type == CliffordOperatorType::h || type == CliffordOperatorType::s ||
            type == CliffordOperatorType::sdg) {
            fmt::println("  [{}] {}({})", i, to_string(type), qubits[0]);
        }
    }
}

void print_block_layers(NcfBlock const& b) {
    print_clifford_layer("C†", b.c_dagger().ops());
    fmt::println("inner ({} rotations):", b.inner_rotations().size());
    for (auto const& r : b.inner_rotations()) fmt::println("  {:c}", r);
    print_clifford_layer("C", b.c_forward().ops());
}

}  // namespace

void register_ncf_from_tableau(NcfMgr& ncf_mgr, TableauMgr const& tableau_mgr, Tableau const* pre_ncf) {
    auto prog = build_ncf_program_from_tableau(*tableau_mgr.get(), tableau_mgr.focused_id(), pre_ncf);
    if (!prog) return;
    auto id = ncf_mgr.get_next_id();
    ncf_mgr.add(id, std::move(prog));
    ncf_mgr.checkout(id);
    print_ncf_fusion_report(ncf_mgr.get()->fusion_report());
    fmt::println("NcfProgram {} ready — try: ncf print --summary", id);
}

dvlab::Command ncf_from_tableau_cmd(NcfMgr& ncf_mgr, TableauMgr& tableau_mgr) {
    return {"from-tableau",
            [&](ArgumentParser& parser) {
                parser.description("Build NcfProgram from NCF-shaped Tableau");
                parser.add_argument<size_t>("tableau_id").nargs(NArgsOption::optional);
            },
            [&](ArgumentParser const& parser) {
                if (parser.parsed("tableau_id")) tableau_mgr.checkout(parser.get<size_t>("tableau_id"));
                if (!dvlab::utils::mgr_has_data(tableau_mgr)) return dvlab::CmdExecResult::error;
                register_ncf_from_tableau(ncf_mgr, tableau_mgr, nullptr);
                return ncf_mgr.get() ? dvlab::CmdExecResult::done : dvlab::CmdExecResult::error;
            }};
}

dvlab::Command ncf_print_cmd(NcfMgr& ncf_mgr) {
    return {"print",
            [&](ArgumentParser& parser) {
                parser.add_argument<bool>("--summary").action(store_true);
                parser.add_argument<bool>("--cost").action(store_true);
                parser.add_argument<bool>("--blocks").action(store_true);
            },
            [&](ArgumentParser const& parser) {
                if (!dvlab::utils::mgr_has_data(ncf_mgr)) return dvlab::CmdExecResult::error;
                auto* p = ncf_mgr.get();
                // --summary / --cost currently share the fusion-report view.
                (void)parser.get<bool>("--summary");
                (void)parser.get<bool>("--cost");
                p->fusion_report().print();
                if (parser.get<bool>("--blocks")) {
                    for (size_t id : p->order()) {
                        if (auto const* b = p->block(id)) print_block_stats(*b);
                    }
                }
                return dvlab::CmdExecResult::done;
            }};
}

dvlab::Command ncf_block_cmd(NcfMgr& ncf_mgr) {
    auto cmd = dvlab::Command{"block", [&](ArgumentParser& p) { p.description("NCF block commands"); },
                              [&](ArgumentParser const&) { return dvlab::CmdExecResult::done; }};
    cmd.add_subcommand("block-group", {"checkout",
                                       [&](ArgumentParser& parser) { parser.add_argument<size_t>("id"); },
                                       [&](ArgumentParser const& parser) {
                                           if (!dvlab::utils::mgr_has_data(ncf_mgr)) return dvlab::CmdExecResult::error;
                                           ncf_mgr.get()->set_focused_block_id(parser.get<size_t>("id"));
                                           return dvlab::CmdExecResult::done;
                                       }});
    cmd.add_subcommand("block-group", {"print",
                                       [&](ArgumentParser& parser) {
                                           parser.add_argument<size_t>("id").nargs(NArgsOption::optional);
                                           parser.add_argument<std::string>("--view").default_value("pauli");
                                       },
                                       [&](ArgumentParser const& parser) {
                                           if (!dvlab::utils::mgr_has_data(ncf_mgr)) return dvlab::CmdExecResult::error;
                                           size_t bid = parser.parsed("id") ? parser.get<size_t>("id")
                                                                             : ncf_mgr.get()->focused_block_id();
                                           auto const* b = ncf_mgr.get()->block(bid);
                                           if (!b) return dvlab::CmdExecResult::error;
                                           auto view = parser.get<std::string>("--view");
                                           if (view == "stats") print_block_stats(*b);
                                           else if (view == "inner") {
                                               for (auto const& r : b->inner_rotations()) fmt::println("  {:c}", r);
                                           } else if (view == "layers") {
                                               print_block_layers(*b);
                                           } else if (view == "parity") {
                                               fmt::println("C†:\n{}", b->c_dagger().parity().to_bit_string());
                                               fmt::println("C:\n{}", b->c_forward().parity().to_bit_string());
                                           } else if (view == "cx-graph") {
                                               auto const& g = b->c_forward().cx_graph();
                                               for (auto const& e : g.edges) {
                                                   fmt::println("cx({}, {}) [#{}]", e.control, e.target, e.seq_index);
                                               }
                                               if (!g.self_inverse_pairs.empty()) {
                                                   fmt::println("self-cancel pairs:");
                                                   for (auto const& [i, j] : g.self_inverse_pairs) {
                                                       auto const& e = g.edges[i];
                                                       fmt::println("  [{}]↔[{}] cx({}, {})", i, j, e.control, e.target);
                                                   }
                                               }
                                           } else print_block_pauli(*b);
                                           return dvlab::CmdExecResult::done;
                                       }});
    return cmd;
}

dvlab::Command ncf_analyze_cmd(NcfMgr& ncf_mgr) {
    return {"analyze",
            [&](ArgumentParser& parser) {
                auto sub = parser.add_subparsers("method").required(true);
                sub.add_parser("commute").description("Print block commute layers from original Paulis");
            },
            [&](ArgumentParser const& parser) {
                if (!dvlab::utils::mgr_has_data(ncf_mgr)) return dvlab::CmdExecResult::error;
                auto* p     = ncf_mgr.get();
                auto* cache = p->ensure_analysis_cache();
                if (parser.get<std::string>("method") == "commute") {
                    for (auto const& layer : cache->commute_graph(*p).layers()) {
                        fmt::println("layer: {}", fmt::join(layer, ", "));
                    }
                }
                return dvlab::CmdExecResult::done;
            }};
}

dvlab::Command ncf_to_qcir_cmd(NcfMgr& ncf_mgr, qcir::QCirMgr& qcir_mgr) {
    return {"to-qcir",
            [&](ArgumentParser& parser) { parser.add_argument<std::string>("-r", "--rotation").default_value("ncf"); },
            [&](ArgumentParser const& parser) {
                if (!dvlab::utils::mgr_has_data(ncf_mgr)) return dvlab::CmdExecResult::error;
                auto qc = ncf_program_to_qcir(*ncf_mgr.get(), parser.get<std::string>("-r"), "hopt");
                if (!qc) return dvlab::CmdExecResult::error;
                qcir_mgr.add(qcir_mgr.get_next_id(), std::make_unique<qcir::QCir>(std::move(*qc)));
                return dvlab::CmdExecResult::done;
            }};
}

dvlab::Command ncf_write_cmd(NcfMgr& ncf_mgr) {
    return {"write",
            [&](ArgumentParser& parser) { parser.add_argument<std::string>("filepath"); },
            [&](ArgumentParser const& parser) {
                if (!dvlab::utils::mgr_has_data(ncf_mgr)) return dvlab::CmdExecResult::error;
                auto* p = ncf_mgr.get();
                std::ofstream out(parser.get<std::string>("filepath"));
                out << "{\n";
                out << fmt::format("  \"n_qubits\": {},\n", p->n_qubits());
                out << "  \"order\": [";
                for (size_t i = 0; i < p->order().size(); ++i) {
                    if (i > 0) out << ", ";
                    out << p->order()[i];
                }
                out << "],\n  \"blocks\": [\n";
                for (size_t i = 0; i < p->order().size(); ++i) {
                    if (auto const* b = p->block(p->order()[i])) {
                        if (i > 0) out << ",\n";
                        out << fmt::format("    {{\"id\": {}, \"indices\": [{}]}}",
                                           b->id(), fmt::join(b->original_indices(), ", "));
                    }
                }
                out << "\n  ]\n}\n";
                return dvlab::CmdExecResult::done;
            }};
}

dvlab::Command ncf_cmd(NcfMgr& ncf_mgr, TableauMgr& tableau_mgr, qcir::QCirMgr& qcir_mgr) {
    auto cmd = dvlab::utils::mgr_root_cmd(ncf_mgr);
    cmd.add_subcommand("ncf-cmd-group", dvlab::utils::mgr_list_cmd(ncf_mgr));
    cmd.add_subcommand("ncf-cmd-group", dvlab::utils::mgr_checkout_cmd(ncf_mgr));
    cmd.add_subcommand("ncf-cmd-group", dvlab::utils::mgr_copy_cmd(ncf_mgr));
    cmd.add_subcommand("ncf-cmd-group", dvlab::utils::mgr_delete_cmd(ncf_mgr));
    cmd.add_subcommand("ncf-cmd-group", ncf_from_tableau_cmd(ncf_mgr, tableau_mgr));
    cmd.add_subcommand("ncf-cmd-group", ncf_print_cmd(ncf_mgr));
    cmd.add_subcommand("ncf-cmd-group", ncf_block_cmd(ncf_mgr));
    cmd.add_subcommand("ncf-cmd-group", ncf_analyze_cmd(ncf_mgr));
    cmd.add_subcommand("ncf-cmd-group", ncf_to_qcir_cmd(ncf_mgr, qcir_mgr));
    cmd.add_subcommand("ncf-cmd-group", ncf_write_cmd(ncf_mgr));
    return cmd;
}

bool add_ncf_command(dvlab::CommandLineInterface& cli, NcfMgr& ncf_mgr, TableauMgr& tableau_mgr,
                     qcir::QCirMgr& qcir_mgr) {
    return cli.add_command(ncf_cmd(ncf_mgr, tableau_mgr, qcir_mgr));
}

}  // namespace qsyn::experimental
