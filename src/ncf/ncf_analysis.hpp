/**
 * @file ncf_analysis.hpp
 * @brief Block commute graph / layers over NcfProgram.
 *        CLI: ncf analyze commute
 */

#pragma once

#include <memory>
#include <vector>

#include "ncf/ncf_types.hpp"

namespace qsyn::experimental {

class NcfProgram;
class NcfBlock;

class NcfCommuteGraph {
public:
    explicit NcfCommuteGraph(NcfProgram const& program);

    bool commute_pauli(size_t a, size_t b) const;
    std::vector<std::vector<size_t>> layers() const;
    std::vector<std::vector<bool>> matrix() const;

private:
    size_t _n = 0;
    std::vector<std::vector<bool>> _commute;
};

class NcfAnalysisCache {
public:
    NcfCommuteGraph const& commute_graph(NcfProgram const& program);
    std::vector<NcfJunction> const& junctions(NcfProgram const& program);
    std::vector<NcfScheduleCandidate> const& schedules(NcfProgram const& program, std::string const& mode);

private:
    std::unique_ptr<NcfCommuteGraph> _commute;
    std::unique_ptr<std::vector<NcfJunction>> _junctions;
    std::unique_ptr<std::vector<NcfScheduleCandidate>> _schedules;
    std::string _schedule_mode;
};

[[nodiscard]] NcfJunction analyze_junction(NcfBlock const& left, NcfBlock const& right);
[[nodiscard]] std::vector<NcfJunction> junctions_along_order(NcfProgram const& program, std::vector<size_t> const& order);
[[nodiscard]] size_t predicted_shell_cx(NcfProgram const& program, std::vector<size_t> const& order);
[[nodiscard]] size_t seam_cx_saved(NcfProgram const& program, std::vector<size_t> const& order);
[[nodiscard]] bool reorder_commute_valid(NcfProgram const& program, NcfCommuteGraph const& cg,
                                         std::vector<size_t> const& order);

}  // namespace qsyn::experimental
