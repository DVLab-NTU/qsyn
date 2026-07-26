#include "ncf/ncf_convert.hpp"

#include <regex>
#include <spdlog/spdlog.h>

#include "convert/tableau_to_qcir.hpp"
#include "ncf/ncf_block.hpp"
#include "ncf/ncf_program.hpp"
#include "qcir/qcir.hpp"

namespace qsyn::experimental {

namespace {

std::unique_ptr<StabilizerTableauSynthesisStrategy> make_clifford_strategy(std::string const& name) {
    if (name.starts_with("hopt")) return std::make_unique<HOptSynthesisStrategy>();
    return std::make_unique<AGSynthesisStrategy>();
}

std::unique_ptr<PauliRotationsSynthesisStrategy> make_rotation_strategy(std::string const& name) {
    if (name.starts_with("ncf")) return std::make_unique<NcfMergePauliRotationsSynthesisStrategy>();
    return std::make_unique<NaivePauliRotationsSynthesisStrategy>();
}

NcfBlockKind kind_from_indices(std::vector<size_t> const& idx) {
    if (idx.size() == 1) return NcfBlockKind::singleton;
    if (idx.size() == 2) return NcfBlockKind::anti_pair;
    if (idx.size() == 3) return NcfBlockKind::anti_triple;
    return NcfBlockKind::unknown;
}

std::optional<size_t> infer_pivot(std::vector<PauliRotation> const& rots) {
    if (rots.empty()) return std::nullopt;
    auto const n = rots.front().n_qubits();
    std::optional<size_t> q;
    for (auto const& r : rots) {
        size_t count = 0;
        size_t qq    = 0;
        for (size_t i = 0; i < n; ++i) {
            if (r.get_pauli_type(i) != Pauli::i) {
                ++count;
                qq = i;
            }
        }
        if (count != 1) return std::nullopt;
        if (!q) q = qq;
        else if (*q != qq) return std::nullopt;
    }
    return q;
}

}  // namespace

std::optional<std::vector<size_t>> parse_original_indices(std::string const& label) {
    static std::regex re(R"(#(\d+))");
    std::vector<size_t> out;
    for (std::sregex_iterator it(label.begin(), label.end(), re), end; it != end; ++it) {
        out.push_back(static_cast<size_t>(std::stoul((*it)[1].str())));
    }
    if (out.empty()) return std::nullopt;
    return out;
}

NcfGateCost gate_cost_from_tableau(Tableau const& tableau, std::string const& rotation_strategy,
                                   std::string const& clifford_strategy) {
    NcfGateCost cost;
    auto cstr = make_clifford_strategy(clifford_strategy);
    auto rstr = make_rotation_strategy(rotation_strategy);
    auto qcir = to_qcir(tableau, *cstr, *rstr);
    if (!qcir) return cost;
    auto stat = get_gate_statistics(*qcir);
    cost.cx  = stat.contains("cx") ? stat.at("cx") : 0;
    cost.rz  = stat.contains("rz") ? stat.at("rz") : 0;
    cost.h   = stat.contains("h") ? stat.at("h") : 0;
    cost.s   = stat.contains("s") ? stat.at("s") : 0;
    cost.sdg = stat.contains("sdg") ? stat.at("sdg") : 0;
    cost.x   = stat.contains("x") ? stat.at("x") : 0;
    cost.y   = stat.contains("y") ? stat.at("y") : 0;
    cost.z   = stat.contains("z") ? stat.at("z") : 0;
    cost.clifford_total = stat.contains("clifford") ? stat.at("clifford") : 0;
    cost.non_clifford   = stat.contains("1-qubit") ? stat.at("1-qubit") : cost.rz;
    cost.total_gates    = qcir->get_num_gates();
    cost.depth          = qcir->calculate_depth();
    return cost;
}

std::unique_ptr<NcfProgram> build_ncf_program_from_tableau(Tableau const& post_ncf, size_t source_tableau_id,
                                                           Tableau const* pre_ncf) {
    if (!has_ncf_canonical_shape(post_ncf)) {
        spdlog::error("Tableau does not have NCF canonical shape [Stab][C†][R][C]…");
        return nullptr;
    }
    auto program = std::make_unique<NcfProgram>(source_tableau_id, post_ncf.n_qubits());
    program->set_front_clifford(std::get<StabilizerTableau>(post_ncf.front()));

    size_t block_id = 0;
    for (size_t i = 1; i + 2 < post_ncf.size(); i += 3) {
        auto const& cd_st = std::get<StabilizerTableau>(post_ncf[i]);
        auto const& rots  = std::get<std::vector<PauliRotation>>(post_ncf[i + 1]);
        (void)cd_st;
        CliffordOperatorString c_dagger = post_ncf.get_block_ops(i).value_or(CliffordOperatorString{});
        CliffordOperatorString c_forward = post_ncf.get_block_ops(i + 2).value_or(CliffordOperatorString{});

        auto indices = parse_original_indices(post_ncf.get_block_label(i + 1)).value_or(std::vector<size_t>{});
        if (indices.empty()) indices.push_back(block_id);

        std::vector<PauliRotation> pauli_before;
        if (pre_ncf && pre_ncf->size() >= 2 && std::holds_alternative<std::vector<PauliRotation>>(pre_ncf->back())) {
            auto const& all = std::get<std::vector<PauliRotation>>(pre_ncf->back());
            for (size_t idx : indices) {
                if (idx < all.size()) pauli_before.push_back(all[idx]);
            }
        }

        auto pivot = infer_pivot(rots);
        auto kind  = kind_from_indices(indices);
        program->add_block(std::make_unique<NcfBlock>(block_id, indices, pauli_before, rots, rots, pivot, kind,
                                                      c_dagger, c_forward, post_ncf.n_qubits()));
        ++block_id;
    }

    program->fusion_report().n_blocks      = program->n_blocks();
    program->fusion_report().n_pauli_terms = pre_ncf && pre_ncf->size() >= 2 && std::holds_alternative<std::vector<PauliRotation>>(pre_ncf->back())
                                                 ? std::get<std::vector<PauliRotation>>(pre_ncf->back()).size()
                                                 : program->n_blocks();
    if (pre_ncf) {
        program->fusion_report().before = gate_cost_from_tableau(*pre_ncf, "naive", "hopt");
    }
    program->fusion_report().after = gate_cost_from_tableau(post_ncf, "ncf", "hopt");
    program->set_filename("ncf-program");
    program->add_procedure("from-tableau");
    return program;
}

std::unique_ptr<Tableau> ncf_program_to_tableau(NcfProgram const& program) {
    Tableau tab(program.n_qubits());
    tab.erase(tab.begin(), tab.end());
    if (program.front_clifford()) {
        tab.push_back(SubTableau{*program.front_clifford()});
    } else {
        tab.push_back(SubTableau{StabilizerTableau{program.n_qubits()}});
    }
    for (size_t bid : program.order()) {
        auto const* b = program.block(bid);
        if (!b) continue;
        auto block_tab = b->to_tableau();
        for (size_t i = 1; i < block_tab->size(); ++i) {
            tab.push_back((*block_tab)[i]);
            if (auto const& ops = block_tab->get_block_ops(i); ops) {
                tab.set_block_ops(tab.size() - 1, *ops);
            }
            if (!block_tab->get_block_label(i).empty()) {
                tab.set_block_label(tab.size() - 1, block_tab->get_block_label(i));
            }
        }
    }
    return std::make_unique<Tableau>(std::move(tab));
}

std::optional<qcir::QCir> ncf_program_to_qcir(NcfProgram const& program, std::string const& rotation_strategy,
                                              std::string const& clifford_strategy) {
    auto tab = ncf_program_to_tableau(program);
    if (!tab) return std::nullopt;
    return to_qcir(*tab, *make_clifford_strategy(clifford_strategy), *make_rotation_strategy(rotation_strategy));
}

}  // namespace qsyn::experimental
