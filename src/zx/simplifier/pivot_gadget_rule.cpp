/****************************************************************************
  PackageName  [ simplifier ]
  Synopsis     [ Pivot Gadget Rule Definition ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2023 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./zx_rules_template.hpp"

using namespace qsyn::zx;

using MatchType = PivotGadgetRule::MatchType;

extern size_t VERBOSE;

std::vector<MatchType> PivotGadgetRule::find_matches(ZXGraph const& graph) const {
    std::vector<MatchType> matches;

    if (VERBOSE >= 8) std::cout << "> match...\n";

    size_t count = 0;

    std::unordered_set<ZXVertex*> taken;

    graph.for_each_edge([&count, &taken, &matches, this](EdgePair const& epair) {
        if (epair.second != EdgeType::hadamard) return;

        ZXVertex* vs = epair.first.first;
        ZXVertex* vt = epair.first.second;

        if (taken.contains(vs) || taken.contains(vt)) return;

        if (VERBOSE == 9) std::cout << "\n-----------\n\n"
                                    << "Edge " << count << ": " << vs->get_id() << " " << vt->get_id() << "\n";

        if (!vs->is_z()) {
            taken.insert(vs);
            return;
        }
        if (!vt->is_z()) {
            taken.insert(vt);
            return;
        }

        if (VERBOSE == 9) std::cout << "(1) type pass\n";

        bool vs_is_n_pi = (vs->get_phase().denominator() == 1);
        bool vt_is_n_pi = (vt->get_phase().denominator() == 1);

        // if both n*pi --> ordinary pivot rules
        // if both not, --> maybe pivot double-boundary
        if (vs_is_n_pi == vt_is_n_pi) return;

        if (!vs_is_n_pi && vt_is_n_pi) std::swap(vs, vt);  // if vs is not n*pi but vt is, should extract vs as gadget instead

        if (VERBOSE == 9) std::cout << "(2) phase pass\n";

        // REVIEW - check ground conditions

        if (vt->get_num_neighbors() == 1) {  // early return: (vs, vt) is a phase gadget
            taken.insert(vs);
            taken.insert(vt);
            return;
        }

        for (const auto& [v, _] : vs->get_neighbors()) {
            if (!v->is_z()) return;             // vs is not internal or not graph-like
            if (v->get_num_neighbors() == 1) {  // (vs, v) is a phase gadget
                taken.insert(vs);
                taken.insert(v);
                return;
            }
        }
        for (const auto& [v, _] : vt->get_neighbors()) {
            if (!v->is_z()) return;  // vt is not internal or not graph-like
        }

        if (VERBOSE == 9) std::cout << "(3) good match\n";

        // Both vs and vt are interior
        if (VERBOSE >= 8) std::cout << "Both vertices are both interior: " << vs->get_id() << " " << vt->get_id() << std::endl;

        taken.insert(vs);
        taken.insert(vt);
        for (auto& [v, _] : vs->get_neighbors()) taken.insert(v);
        for (auto& [v, _] : vt->get_neighbors()) taken.insert(v);

        matches.emplace_back(vs, vt);
    });

    return matches;
}

void PivotGadgetRule::apply(ZXGraph& graph, std::vector<MatchType> const& matches) const {
    for (auto& [_, v] : matches) {
        // REVIEW - scalar add power
        if (v->get_phase().denominator() != 1) {
            graph.gadgetize_phase(v);
        }
    }

    PivotRuleInterface::apply(graph, matches);
}
