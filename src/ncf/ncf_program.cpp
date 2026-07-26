#include "ncf/ncf_program.hpp"

#include <algorithm>

#include "ncf/ncf_analysis.hpp"
#include "ncf/ncf_convert.hpp"

namespace qsyn::experimental {

NcfProgram::NcfProgram(size_t source_tableau_id, size_t n_qubits)
    : _source_tableau_id(source_tableau_id), _n_qubits(n_qubits) {}

NcfProgram::NcfProgram(NcfProgram const& other)
    : _source_tableau_id(other._source_tableau_id),
      _n_qubits(other._n_qubits),
      _front_clifford(other._front_clifford),
      _order(other._order),
      _focused_block_id(other._focused_block_id),
      _fusion_report(other._fusion_report),
      _filename(other._filename),
      _procedures(other._procedures) {
    for (auto const& b : other._blocks) {
        if (b) _blocks.push_back(b->clone());
    }
}

NcfProgram::~NcfProgram() = default;

void NcfProgram::add_block(std::unique_ptr<NcfBlock> block) {
    if (!block) return;
    _order.push_back(block->id());
    _blocks.push_back(std::move(block));
}

NcfBlock const* NcfProgram::block(size_t id) const {
    for (auto const& b : _blocks) {
        if (b && b->id() == id) return b.get();
    }
    return nullptr;
}

NcfBlock* NcfProgram::block(size_t id) {
    for (auto& b : _blocks) {
        if (b && b->id() == id) return b.get();
    }
    return nullptr;
}

void NcfProgram::invalidate_cache() { _cache.reset(); }

NcfAnalysisCache* NcfProgram::ensure_analysis_cache() {
    if (!_cache) _cache = std::make_unique<NcfAnalysisCache>();
    return _cache.get();
}

bool NcfProgram::reorder(std::vector<size_t> new_order, bool check_commute) {
    if (new_order.size() != _blocks.size()) return false;
    if (check_commute) {
        auto* cache = ensure_analysis_cache();
        auto const& cg = cache->commute_graph(*this);
        auto pos_in = [&](size_t id) {
            auto it = std::find(_order.begin(), _order.end(), id);
            return static_cast<size_t>(std::distance(_order.begin(), it));
        };
        for (size_t i = 0; i < new_order.size(); ++i) {
            for (size_t j = i + 1; j < new_order.size(); ++j) {
                if (!cg.commute_pauli(new_order[i], new_order[j]) &&
                    pos_in(new_order[i]) > pos_in(new_order[j])) {
                    return false;
                }
            }
        }
    }
    _order = std::move(new_order);
    invalidate_cache();
    add_procedure("reorder");
    return true;
}

size_t NcfProgram::merge_shells(bool dry_run) {
    auto* cache = ensure_analysis_cache();
    auto const& junctions = cache->junctions(*this);
    size_t saved = 0;
    for (auto const& j : junctions) {
        if (j.exactly_identity) saved += j.cx_saved_if_merged;
    }
    if (!dry_run && saved > 0) {
        add_procedure("merge-shells");
        _fusion_report.after.cx = _fusion_report.after.cx > saved ? _fusion_report.after.cx - saved : 0;
    }
    return saved;
}

std::unique_ptr<NcfProgram> NcfProgram::clone() const {
    auto p = std::make_unique<NcfProgram>(_source_tableau_id, _n_qubits);
    p->_front_clifford = _front_clifford;
    p->_order           = _order;
    p->_fusion_report   = _fusion_report;
    p->_filename        = _filename;
    p->_procedures      = _procedures;
    for (auto const& b : _blocks) {
        if (b) p->_blocks.push_back(b->clone());
    }
    return p;
}

std::unique_ptr<Tableau> NcfProgram::to_tableau() const { return ncf_program_to_tableau(*this); }

void print_ncf_fusion_report(NcfFusionReport const& report) { report.print(); }

}  // namespace qsyn::experimental
