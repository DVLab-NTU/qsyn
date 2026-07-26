#include "ncf/ncf_block.hpp"

#include <fmt/core.h>
#include <unordered_map>

namespace qsyn::experimental {

namespace {

size_t count_cx(CliffordOperatorString const& ops) {
    return std::ranges::count_if(ops, [](auto const& op) { return op.first == CliffordOperatorType::cx; });
}

size_t count_hs(CliffordOperatorString const& ops) {
    return std::ranges::count_if(ops, [](auto const& op) {
        return op.first == CliffordOperatorType::h || op.first == CliffordOperatorType::s ||
               op.first == CliffordOperatorType::sdg;
    });
}

NcfBlockKind kind_from_size(size_t n) {
    if (n == 1) return NcfBlockKind::singleton;
    if (n == 2) return NcfBlockKind::anti_pair;
    if (n == 3) return NcfBlockKind::anti_triple;
    return NcfBlockKind::unknown;
}

}  // namespace

StabilizerTableau ops_to_stabilizer_tableau(size_t n_qubits, CliffordOperatorString const& ops) {
    StabilizerTableau st{n_qubits};
    st.prepend(ops);
    return st;
}

NcfCxGraph build_cx_graph(CliffordOperatorString const& ops) {
    NcfCxGraph g;
    std::unordered_map<size_t, size_t> out_degree;
    for (size_t i = 0; i < ops.size(); ++i) {
        if (ops[i].first != CliffordOperatorType::cx) continue;
        NcfCxEdge e{ops[i].second[0], ops[i].second[1], i};
        g.edges.push_back(e);
        out_degree[e.control]++;
    }
    if (g.edges.empty()) {
        g.topology = NcfCxGraph::Topology::general;
        return g;
    }
    if (g.edges.size() == 1) {
        g.topology = NcfCxGraph::Topology::ladder;
        return g;
    }
    std::optional<size_t> root;
    size_t max_deg = 0;
    for (auto const& [q, deg] : out_degree) {
        if (deg > max_deg) {
            max_deg = deg;
            root    = q;
        }
    }
    if (root && max_deg + 1 == g.edges.size()) {
        g.root     = *root;
        g.topology = NcfCxGraph::Topology::star;
    } else {
        g.topology = NcfCxGraph::Topology::general;
    }
    for (size_t i = 0; i < g.edges.size(); ++i) {
        for (size_t j = i + 1; j < g.edges.size(); ++j) {
            if (g.edges[i].control == g.edges[j].control && g.edges[i].target == g.edges[j].target) {
                g.self_inverse_pairs.emplace_back(i, j);
            }
        }
    }
    return g;
}

NcfShellSide::NcfShellSide(CliffordOperatorString ops, size_t n_qubits)
    : _ops(std::move(ops)), _n_qubits(n_qubits) {}

StabilizerTableau const& NcfShellSide::parity() const {
    if (!_parity) _parity = std::make_unique<StabilizerTableau>(ops_to_stabilizer_tableau(_n_qubits, _ops));
    return *_parity;
}

void NcfShellSide::ensure_cx_graph() const {
    if (_cx_graph) return;
    _cx_graph = build_cx_graph(_ops);
}

NcfCxGraph const& NcfShellSide::cx_graph() const {
    ensure_cx_graph();
    return *_cx_graph;
}

std::vector<std::pair<size_t, size_t>> const& NcfShellSide::self_cancel_pairs() const {
    ensure_cx_graph();
    return _cx_graph->self_inverse_pairs;
}

NcfBlock::NcfBlock(size_t id,
                   std::vector<size_t> original_indices,
                   std::vector<PauliRotation> pauli_before,
                   std::vector<PauliRotation> pauli_after,
                   std::vector<PauliRotation> inner,
                   std::optional<size_t> pivot,
                   NcfBlockKind kind,
                   CliffordOperatorString c_dagger,
                   CliffordOperatorString c_forward,
                   size_t n_qubits)
    : _id(id),
      _kind(kind),
      _original_indices(std::move(original_indices)),
      _pauli_before(std::move(pauli_before)),
      _pauli_after(std::move(pauli_after)),
      _inner(std::move(inner)),
      _pivot(pivot),
      _c_dagger(std::move(c_dagger), n_qubits),
      _c_forward(std::move(c_forward), n_qubits) {}

std::unique_ptr<Tableau> NcfBlock::to_tableau() const {
    size_t const n = _inner.empty() ? (_pauli_after.empty() ? 1 : _pauli_after.front().n_qubits()) : _inner.front().n_qubits();
    Tableau tab(n);
    tab.erase(tab.begin(), tab.end());

    StabilizerTableau cd(n);
    cd.prepend(_c_dagger.ops());
    tab.push_back(SubTableau{std::move(cd)});
    tab.set_block_ops(tab.size() - 1, _c_dagger.ops());

    tab.push_back(SubTableau{_inner.empty() ? _pauli_after : _inner});
    std::string label = fmt::format("NCF block {} (original ", _id);
    for (size_t k = 0; k < _original_indices.size(); ++k) {
        if (k > 0) label += ", ";
        label += fmt::format("#{}", _original_indices[k]);
    }
    label += ")";
    tab.set_block_label(tab.size() - 1, label);

    StabilizerTableau cf(n);
    cf.prepend(_c_forward.ops());
    tab.push_back(SubTableau{std::move(cf)});
    tab.set_block_ops(tab.size() - 1, _c_forward.ops());
    return std::make_unique<Tableau>(std::move(tab));
}

NcfBlockStats NcfBlock::stats() const {
    NcfBlockStats s;
    s.cx_prefix = count_cx(_c_dagger.ops());
    s.cx_suffix = count_cx(_c_forward.ops());
    s.cx_total  = s.cx_prefix + s.cx_suffix;
    s.inner_rz_count = _inner.size();
    s.hs_count = count_hs(_c_dagger.ops()) + count_hs(_c_forward.ops());
    s.pivot    = _pivot;
    return s;
}

std::unique_ptr<NcfBlock> NcfBlock::clone() const {
    return std::make_unique<NcfBlock>(_id, _original_indices, _pauli_before, _pauli_after, _inner, _pivot, _kind,
                                      _c_dagger.ops(), _c_forward.ops(),
                                      _inner.empty() ? _pauli_after.front().n_qubits() : _inner.front().n_qubits());
}

}  // namespace qsyn::experimental
