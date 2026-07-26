/**
 * @file ncf_block.hpp
 * @brief One fused NCF group: [C†][R'][C], Pauli before/after, shell CX graphs.
 *        CLI: ncf block print --view pauli|stats|inner|layers|parity|cx-graph
 */

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "ncf/ncf_types.hpp"
#include "tableau/tableau.hpp"

namespace qsyn::experimental {

struct NcfBlockStats {
    size_t cx_prefix = 0;
    size_t cx_suffix = 0;
    size_t cx_total  = 0;
    size_t inner_rz_count = 0;
    size_t hs_count = 0;
    std::optional<size_t> pivot;
};

NcfCxGraph build_cx_graph(CliffordOperatorString const& ops);
StabilizerTableau ops_to_stabilizer_tableau(size_t n_qubits, CliffordOperatorString const& ops);

class NcfShellSide {
public:
    explicit NcfShellSide(CliffordOperatorString ops, size_t n_qubits);

    CliffordOperatorString const& ops() const { return _ops; }
    StabilizerTableau const& parity() const;
    NcfCxGraph const& cx_graph() const;
    std::vector<std::pair<size_t, size_t>> const& self_cancel_pairs() const;

private:
    CliffordOperatorString _ops;
    size_t _n_qubits;
    mutable std::unique_ptr<StabilizerTableau> _parity;
    mutable std::optional<NcfCxGraph> _cx_graph;
    mutable std::vector<std::pair<size_t, size_t>> _self_cancel_pairs;
    void ensure_cx_graph() const;
};

class NcfBlock {
public:
    NcfBlock(size_t id,
             std::vector<size_t> original_indices,
             std::vector<PauliRotation> pauli_before,
             std::vector<PauliRotation> pauli_after,
             std::vector<PauliRotation> inner,
             std::optional<size_t> pivot,
             NcfBlockKind kind,
             CliffordOperatorString c_dagger,
             CliffordOperatorString c_forward,
             size_t n_qubits);

    size_t id() const { return _id; }
    NcfBlockKind kind() const { return _kind; }
    std::vector<size_t> const& original_indices() const { return _original_indices; }
    std::vector<PauliRotation> const& pauli_before() const { return _pauli_before; }
    std::vector<PauliRotation> const& pauli_after() const { return _pauli_after; }
    std::vector<PauliRotation> const& inner_rotations() const { return _inner; }
    std::optional<size_t> pivot() const { return _pivot; }
    NcfShellSide const& c_dagger() const { return _c_dagger; }
    NcfShellSide const& c_forward() const { return _c_forward; }

    std::unique_ptr<Tableau> to_tableau() const;
    NcfBlockStats stats() const;

    std::unique_ptr<NcfBlock> clone() const;

private:
    size_t _id;
    NcfBlockKind _kind;
    std::vector<size_t> _original_indices;
    std::vector<PauliRotation> _pauli_before;
    std::vector<PauliRotation> _pauli_after;
    std::vector<PauliRotation> _inner;
    std::optional<size_t> _pivot;
    NcfShellSide _c_dagger;
    NcfShellSide _c_forward;
};

}  // namespace qsyn::experimental
