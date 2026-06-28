#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace qsyn::experimental {

struct SatSignatureExport;

enum class PauliClassKind : std::uint8_t {
    Fix,
    Sat
};

struct PauliEquivClass {
    std::vector<size_t> members;
    PauliClassKind      kind = PauliClassKind::Sat;
    size_t              rep  = 0;
};

struct PauliColumnReduction {
    size_t                                      G = 0;
    std::vector<PauliEquivClass>               classes;
    std::unordered_map<size_t, size_t>         pid_to_rep;
    std::vector<size_t>                        sat_reps;
    std::unordered_map<size_t, PauliClassKind> kind_by_rep;
    std::unordered_map<size_t, size_t>         fixed_gap_by_rep;

    [[nodiscard]] size_t rep_for(size_t pid) const;
    [[nodiscard]] std::optional<size_t> fixed_gap_for_rep(size_t rep) const;
};

struct AncillaSmtInstance {
    size_t qubit_count   = 0;
    size_t ancilla_count = 0;
    size_t pauli_count   = 0;
    /** Max gadgets sharing a PR bridge column; lower bound on achievable width. */
    size_t max_column_overlap = 0;

    /** Fixed pi from qsyn export, indexed by rank. */
    std::vector<size_t> gadget_order_gids;
    /** Indexed by gid. */
    std::vector<size_t> gadget_ancilla_qubit;
    /** Per gid, pid list. */
    std::vector<std::vector<size_t>> block_left;
    /** Per gid, pid list. */
    std::vector<std::vector<size_t>> block_right;
    PauliColumnReduction reduction;
};

struct AncillaScheduleResult {
    bool        ok      = false;
    size_t      width_w = 0;
    std::string error;

    std::vector<size_t>                        gadget_order_gids;
    std::unordered_map<size_t, size_t>         column_slot;
    std::unordered_set<size_t>                 degadgetizable_gids;
    std::unordered_map<size_t, std::pair<size_t, size_t>> span_by_gid;
};

struct ParsedGadgetOrdering {
    size_t qubit_count   = 0;
    size_t ancilla_count = 0;
    size_t width_w       = 0;
    bool has_width       = false;
    std::vector<size_t> gadget_order_gids;
    /** From ``Span :`` section: gadget id -> (min_i, max_i) gap indices (non-degadgetizable only). */
    std::unordered_map<size_t, size_t> column_slot;
    std::unordered_set<size_t> degadgetizable_gids;
    std::unordered_map<size_t, std::pair<size_t, size_t>> span_by_gid;
    /** True when the ordering contained an explicit ``degadgetizable`` section (even if empty). */
    bool has_degadgetizable_section = false;

    /** `occupied_gadget_gap_time[gid][t] == 1` iff gadget `gid` is busy at gap time `t`. */
    std::vector<std::vector<std::uint8_t>> occupied_gadget_gap_time;
};

AncillaSmtInstance build_ancilla_smt_instance(SatSignatureExport const& sig);
struct AncillaScheduleSolveOptions {
    std::optional<size_t> start_width = std::nullopt;
    bool stop_if_start_unsat = false;
};
AncillaScheduleResult solve_ancilla_schedule(AncillaSmtInstance const& inst,
                                             AncillaScheduleSolveOptions const& options);
AncillaScheduleResult solve_ancilla_schedule(AncillaSmtInstance const& inst);

void finalize_parsed_gadget_ordering(ParsedGadgetOrdering& ord, std::string& err);
ParsedGadgetOrdering to_parsed_ordering(AncillaSmtInstance const& inst,
                                        AncillaScheduleResult const& result);

}  // namespace qsyn::experimental
