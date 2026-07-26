/**
 * @file ncf_program.hpp
 * @brief Ordered NCF blocks + front Clifford; to_tableau for emission.
 *        CLI: ncf print, ncf to-qcir, ncf write.
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ncf/ncf_block.hpp"
#include "ncf/ncf_types.hpp"
#include "tableau/tableau.hpp"

namespace qsyn::experimental {

class NcfAnalysisCache;

class NcfProgram {
public:
    NcfProgram(size_t source_tableau_id, size_t n_qubits);
    NcfProgram(NcfProgram const& other);
    ~NcfProgram();

    size_t source_tableau_id() const { return _source_tableau_id; }
    size_t n_qubits() const { return _n_qubits; }
    size_t n_blocks() const { return _blocks.size(); }
    std::vector<size_t> const& order() const { return _order; }
    NcfFusionReport const& fusion_report() const { return _fusion_report; }
    NcfFusionReport& fusion_report() { return _fusion_report; }

    void set_front_clifford(StabilizerTableau st) { _front_clifford = std::move(st); }
    std::optional<StabilizerTableau> const& front_clifford() const { return _front_clifford; }

    void add_block(std::unique_ptr<NcfBlock> block);
    NcfBlock const* block(size_t id) const;
    NcfBlock* block(size_t id);

    size_t focused_block_id() const { return _focused_block_id; }
    void set_focused_block_id(size_t id) { _focused_block_id = id; }

    void invalidate_cache();
    NcfAnalysisCache const* analysis_cache() const { return _cache.get(); }
    NcfAnalysisCache* ensure_analysis_cache();

    bool reorder(std::vector<size_t> new_order, bool check_commute = true);
    size_t merge_shells(bool dry_run);

    std::unique_ptr<NcfProgram> clone() const;
    std::unique_ptr<Tableau> to_tableau() const;

    std::string const& filename() const { return _filename; }
    void set_filename(std::string f) { _filename = std::move(f); }
    std::vector<std::string> const& procedures() const { return _procedures; }
    void add_procedure(std::string p) { _procedures.push_back(std::move(p)); }

private:
    size_t _source_tableau_id;
    size_t _n_qubits;
    std::optional<StabilizerTableau> _front_clifford;
    std::vector<std::unique_ptr<NcfBlock>> _blocks;
    std::vector<size_t> _order;
    size_t _focused_block_id = 0;
    NcfFusionReport _fusion_report;
    std::unique_ptr<NcfAnalysisCache> _cache;
    std::string _filename;
    std::vector<std::string> _procedures;
};

void print_ncf_fusion_report(NcfFusionReport const& report);

}  // namespace qsyn::experimental
