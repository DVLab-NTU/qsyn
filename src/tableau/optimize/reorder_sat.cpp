/**
 * @file reorder_sat.cpp
 * @brief SAT-driven gadget / Pauli column reorder (export → Z3 → apply); leaves gadgets in place (no degadgetize).
 */

#include "../tableau_optimization.hpp"

#include "tableau/classical_tableau.hpp"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "tableau/tableau.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <stdexcept>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef __unix__
#include <unistd.h>
#endif

namespace qsyn::experimental {

namespace {

enum class ScheduleParseSection { None, GadgetOrder, ColumnSlot, Span };

struct ParsedGadgetOrdering {
    size_t qubit_count  = 0;
    size_t ancilla_count = 0;
    std::vector<size_t> gadget_order_gids;
    std::unordered_map<size_t, size_t> column_slot;
    /// From ``Span :`` section: gadget id -> (min_i, max_i) gap indices.
    std::unordered_map<size_t, std::pair<size_t, size_t>> span_by_gid;

    /// `occupied_gadget_gap_time[gid][t] == 1` iff gadget `gid` is **busy** at gap time `t` per Span `[min_i,max_i]`
    /// (else `0`). `(gid 0 0)` leaves row `gid` all zero.
    std::vector<std::vector<std::uint8_t>> occupied_gadget_gap_time;
};

bool parse_gadget_ordering_file(std::filesystem::path const& path, ParsedGadgetOrdering& out, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = fmt::format("cannot open {}", path.string());
        return false;
    }

    ScheduleParseSection section = ScheduleParseSection::None;
    std::string line;
    while (std::getline(in, line)) {
        if (auto const hash = line.find('#'); hash != std::string::npos) {
            line.resize(hash);
        }
        // trim
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line.erase(line.begin());
        }
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        if (line.starts_with("qubit_count:")) {
            out.qubit_count = static_cast<size_t>(std::stoull(line.substr(std::string_view("qubit_count:").size())));
            continue;
        }
        if (line.starts_with("ancilla_count:")) {
            out.ancilla_count = static_cast<size_t>(std::stoull(line.substr(std::string_view("ancilla_count:").size())));
            continue;
        }
        if (line.starts_with("width:")) {
            continue;
        }
        if (line == "gadget_order") {
            section = ScheduleParseSection::GadgetOrder;
            continue;
        }
        if (line == "column_slot") {
            section = ScheduleParseSection::ColumnSlot;
            continue;
        }
        if (line == "Span :" || line.starts_with("Span")) {
            section = ScheduleParseSection::Span;
            continue;
        }

        if (section == ScheduleParseSection::GadgetOrder) {
            std::istringstream iss(line);
            size_t gid = 0;
            size_t rank = 0;
            if (!(iss >> gid >> rank)) {
                err = fmt::format("bad gadget_order line: {}", line);
                return false;
            }
            if (rank != out.gadget_order_gids.size()) {
                err = fmt::format("gadget_order rank mismatch: expected {}, got {}", out.gadget_order_gids.size(), rank);
                return false;
            }
            out.gadget_order_gids.push_back(gid);
        } else if (section == ScheduleParseSection::ColumnSlot) {
            std::istringstream iss(line);
            size_t pid = 0;
            size_t gap = 0;
            if (!(iss >> pid >> gap)) {
                err = fmt::format("bad column_slot line: {}", line);
                return false;
            }
            out.column_slot[pid] = gap;
        } else if (section == ScheduleParseSection::Span) {
            if (out.gadget_order_gids.empty()) {
                err = "Span data requires gadget_order before Span (need G to size occupied_gadget_gap_time)";
                return false;
            }
            size_t const G = out.gadget_order_gids.size();
            size_t const num_gap_times = G + 1;
            if (out.occupied_gadget_gap_time.empty()) {
                out.occupied_gadget_gap_time.assign(G, std::vector<std::uint8_t>(num_gap_times, 0));
            }

            std::istringstream iss(line);
            size_t gid = 0;
            size_t min_i = 0;
            size_t max_i = 0;
            if (!(iss >> gid >> min_i >> max_i)) {
                err = fmt::format("bad Span line: {}", line);
                return false;
            }
            if (gid >= G) {
                err = fmt::format("Span gid {} >= gadget count {}", gid, G);
                return false;
            }
            if (min_i > max_i) {
                err = fmt::format("Span gid {} has min_i {} > max_i {}", gid, min_i, max_i);
                return false;
            }
            if (max_i >= num_gap_times) {
                err = fmt::format(
                    "Span gid {} max_i {} out of range for gap times [0,{}]", gid, max_i, num_gap_times - 1);
                return false;
            }
            if (out.span_by_gid.count(gid) != 0) {
                err = fmt::format("duplicate Span gid {}", gid);
                return false;
            }
            out.span_by_gid.emplace(gid, std::pair<size_t, size_t>{min_i, max_i});

            if (!(min_i == 0 && max_i == 0)) {
                for (size_t t = min_i; t <= max_i; ++t) {
                    out.occupied_gadget_gap_time[gid][t] = 1;
                }
            }
        } else {
            err = fmt::format("unexpected line before gadget_order: {}", line);
            return false;
        }
    }

    if (out.qubit_count == 0 || out.ancilla_count == 0) {
        err = "missing qubit_count or ancilla_count";
        return false;
    }
    if (out.gadget_order_gids.empty()) {
        err = "empty gadget_order";
        return false;
    }
    if (out.column_slot.empty()) {
        err = "empty column_slot";
        return false;
    }
    return true;
}

std::string shell_single_quote(std::filesystem::path const& p) {
    std::string s = p.string();
    std::string out;
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}

std::filesystem::path resolve_sat_formulation_script() {
    if (char const* env = std::getenv("QSYN_SAT_FORMULATION")) {
        std::filesystem::path p(env);
        if (std::filesystem::is_regular_file(p)) {
            return std::filesystem::weakly_canonical(p);
        }
        spdlog::warn("QSYN_SAT_FORMULATION={} is not a regular file", env);
    }
    std::filesystem::path const cwd = std::filesystem::current_path();
    for (char const* rel : {"ancilla-minimization-with-sat/sat_formulation.py",
                            "../ancilla-minimization-with-sat/sat_formulation.py",
                            "../../ancilla-minimization-with-sat/sat_formulation.py"}) {
        auto p = cwd / rel;
        if (std::filesystem::is_regular_file(p)) {
            return std::filesystem::weakly_canonical(p);
        }
    }
    return {};
}

bool validate_ordering_against_tableau(Tableau const& tableau,
                                      std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets,
                                      ParsedGadgetOrdering const& ord,
                                      std::string& err) {
    if (ord.qubit_count != tableau.n_qubits() || ord.ancilla_count != tableau.n_ancilla()) {
        err = fmt::format("header mismatch: file n={} m={} vs tableau n={} m={}",
                          ord.qubit_count, ord.ancilla_count, tableau.n_qubits(), tableau.n_ancilla());
        return false;
    }

    size_t const G = gadgets.size();
    if (ord.gadget_order_gids.size() != G) {
        err = fmt::format("|gadget_order|={} vs tableau gadgets={}", ord.gadget_order_gids.size(), G);
        return false;
    }

    std::vector<bool> seen_gid(G, false);
    for (size_t gid : ord.gadget_order_gids) {
        if (gid >= G) {
            err = fmt::format("gadget gid {} out of range [0,{})", gid, G);
            return false;
        }
        if (seen_gid[gid]) {
            err = fmt::format("duplicate gid {} in gadget_order", gid);
            return false;
        }
        seen_gid[gid] = true;
    }

    size_t pr_count = 0;
    for (size_t idx = 0; idx < tableau.size(); ++idx) {
        auto const* pr_vec = std::get_if<std::vector<PauliRotation>>(&tableau[idx]);
        if (!pr_vec) {
            continue;
        }
        pr_count += pr_vec->size();
    }
    size_t const num_vertices_pr = G + pr_count;
    if (ord.column_slot.size() != pr_count) {
        err = fmt::format("column_slot count {} vs tableau PR columns {}", ord.column_slot.size(), pr_count);
        return false;
    }
    for (auto const& [pid, gap] : ord.column_slot) {
        if (pid < G || pid >= num_vertices_pr) {
            err = fmt::format("column_slot pid {} out of range [{}, {})", pid, G, num_vertices_pr);
            return false;
        }
        if (gap > G) {
            err = fmt::format("column_slot pid {} gap {} > G={}", pid, gap, G);
            return false;
        }
    }
    for (auto const& [gid, mm] : ord.span_by_gid) {
        if (gid >= G) {
            err = fmt::format("Span gid {} out of range [0,{})", gid, G);
            return false;
        }
        if (mm.first > mm.second) {
            err = fmt::format("Span gid {} min_i {} > max_i {}", gid, mm.first, mm.second);
            return false;
        }
        if (mm.second > G) {
            err = fmt::format("Span gid {} max_i {} > G={}", gid, mm.second, G);
            return false;
        }
    }
    return true;
}

void log_and_export_spans(std::filesystem::path const& ordering_path, ParsedGadgetOrdering const& ord) {
    if (ord.span_by_gid.empty()) {
        spdlog::debug("sat_reorder_apply: no Span section in {}", ordering_path.string());
        return;
    }
    for (auto const& [gid, mm] : ord.span_by_gid) {
        spdlog::info("Span gid={} min_i={} max_i={}", gid, mm.first, mm.second);
    }
    std::filesystem::path out_path = ordering_path;
    out_path += ".span";
    std::ofstream out(out_path);
    if (!out) {
        spdlog::warn("sat_reorder_apply: could not write span export '{}'", out_path.string());
        return;
    }
    out << "Span\n";
    std::vector<size_t> gids;
    gids.reserve(ord.span_by_gid.size());
    for (auto const& kv : ord.span_by_gid) {
        gids.push_back(kv.first);
    }
    std::sort(gids.begin(), gids.end());
    for (size_t gid : gids) {
        auto const& mm = ord.span_by_gid.at(gid);
        out << gid << ' ' << mm.first << ' ' << mm.second << '\n';
    }
    spdlog::info("sat_reorder_apply: exported Span per gid to '{}'", out_path.string());
}

bool permute_pmcs_in_middle_for_sat_order(
    std::vector<SubTableau>& middle,
    size_t G,
    ParsedGadgetOrdering const& ord,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets) {
    if (G == 0) {
        return true;
    }
    if (middle.size() < G) {
        spdlog::error("permute_pmcs_in_middle_for_sat_order: middle size {} < G {}", middle.size(), G);
        return false;
    }

    size_t const tail_start = middle.size() - G;

    // Locate each PMC (by gid) at the tail and each gadget CCC in the prefix.
    std::vector<size_t> pmc_pos(G, 0);
    for (size_t k = 0; k < G; ++k) {
        size_t const idx = tail_start + k;
        auto*         pmc = std::get_if<ClassicalControlTableau>(&middle[idx]);
        if (!pmc || !pmc->is_classical_control()) {
            spdlog::error(
                "permute_pmcs_in_middle_for_sat_order: expected {} tail PMCs at indices [{}..{})",
                G, tail_start, middle.size());
            return false;
        }
        pmc_pos[k] = idx;
    }

    std::vector<size_t> ccc_pos;
    ccc_pos.reserve(G);
    for (size_t idx = 0; idx < tail_start; ++idx) {
        if (auto* ccc = std::get_if<ClassicalControlTableau>(&middle[idx])) {
            if (!ccc->is_gadget()) {
                spdlog::error(
                    "permute_pmcs_in_middle_for_sat_order: unexpected classical-control CCT at prefix index {}", idx);
                return false;
            }
            ccc_pos.push_back(idx);
        } else if (!std::holds_alternative<std::vector<PauliRotation>>(middle[idx])) {
            spdlog::error(
                "permute_pmcs_in_middle_for_sat_order: unexpected prefix block at index {}", idx);
            return false;
        }
    }
    if (ccc_pos.size() != G) {
        spdlog::error(
            "permute_pmcs_in_middle_for_sat_order: expected {} gadget CCCs in prefix, found {}", G, ccc_pos.size());
        return false;
    }

    // Build processing order: ascending (span_max, ancilla_qubit, gid). Only gids with span_max < G move.
    std::vector<size_t> span_max(G, 0);
    for (size_t gid = 0; gid < G; ++gid) {
        auto const it = ord.span_by_gid.find(gid);
        if (it == ord.span_by_gid.end()) {
            spdlog::error("permute_pmcs_in_middle_for_sat_order: Span missing for gid {}", gid);
            return false;
        }
        size_t const max_i = it->second.second;
        if (max_i > G) {
            spdlog::error(
                "permute_pmcs_in_middle_for_sat_order: Span max_i {} for gid {} out of range", max_i, gid);
            return false;
        }
        span_max[gid] = max_i;
    }

    std::vector<size_t> movers;
    movers.reserve(G);
    for (size_t gid = 0; gid < G; ++gid) {
        if (span_max[gid] < G) {
            movers.push_back(gid);
        }
    }
    std::sort(movers.begin(), movers.end(), [&](size_t a, size_t b) {
        if (span_max[a] != span_max[b]) {
            return span_max[a] < span_max[b];
        }
        if (gadgets[a].ancilla_qubit != gadgets[b].ancilla_qubit) {
            return gadgets[a].ancilla_qubit < gadgets[b].ancilla_qubit;
        }
        return a < b;
    });

    for (size_t gid : movers) {
        size_t const p = pmc_pos[gid];
        size_t const i = span_max[gid];
        size_t const t = ccc_pos[i];
        if (p < t) {
            spdlog::error(
                "permute_pmcs_in_middle_for_sat_order: PMC gid {} at index {} is already left of target {}",
                gid, p, t);
            return false;
        }
        if (p == t) {
            continue;
        }

        auto* mover_ptr = std::get_if<ClassicalControlTableau>(&middle[p]);
        if (!mover_ptr) {
            spdlog::error("permute_pmcs_in_middle_for_sat_order: PMC gid {} no longer a CCT at {}", gid, p);
            return false;
        }
        ClassicalControlTableau& mover = *mover_ptr;

        // Walk blocks between target and source in reverse (closest-to-source first) and update the mover PMC.
        for (size_t k = p; k > t; --k) {
            spdlog::debug("permute_pmcs_in_middle_for_sat_order: moving PMC gid {} past index {}", gid, k - 1);
            SubTableau& neighbor = middle[k - 1];
            if (auto* pr = std::get_if<std::vector<PauliRotation>>(&neighbor)) {
                swap(*pr, mover);
            } else if (auto* other = std::get_if<ClassicalControlTableau>(&neighbor)) {
                if (other->is_gadget()) {
                    swap(*other, mover);
                } else {
                    if (!check_swap(*other, mover)) {
                        spdlog::error(
                            "permute_pmcs_in_middle_for_sat_order: non-commuting PMCs encountered while moving gid {}",
                            gid);
                        return false;
                    }
                }
            } else {
                spdlog::error(
                    "permute_pmcs_in_middle_for_sat_order: unexpected block while moving gid {} past index {}",
                    gid, k - 1);
                return false;
            }
        }

        SubTableau moved = std::move(middle[p]);
        middle.erase(middle.begin() + static_cast<std::ptrdiff_t>(p));
        middle.insert(middle.begin() + static_cast<std::ptrdiff_t>(t), std::move(moved));

        // Bookkeeping: blocks at [t .. p-1] shifted right by 1; block at p was removed, block inserted at t.
        for (size_t k = i; k < G; ++k) {
            ++ccc_pos[k];
        }
        for (size_t other_gid = 0; other_gid < G; ++other_gid) {
            size_t& op = pmc_pos[other_gid];
            if (other_gid == gid) {
                op = t;
            } else if (op >= t && op < p) {
                ++op;
            }
        }
    }

    return true;
}

bool validate_classical_controls_against_span_allowlist(
    std::vector<SubTableau> const& middle,
    size_t const G,
    ParsedGadgetOrdering const& ord,
    std::vector<ConstraintGraph::HadamardGadgetPair> const& gadgets) {
    if (ord.occupied_gadget_gap_time.size() != G) {
        spdlog::error(
            "validate_classical_controls_against_span_allowlist: occupied_gadget_gap_time rows {} != G {}",
            ord.occupied_gadget_gap_time.size(),
            G);
        return false;
    }
    for (size_t gid = 0; gid < G; ++gid) {
        if (ord.occupied_gadget_gap_time[gid].size() != G + 1) {
            spdlog::error(
                "validate_classical_controls_against_span_allowlist: row gid {} width {} != G+1 {}",
                gid,
                ord.occupied_gadget_gap_time[gid].size(),
                G + 1);
            return false;
        }
    }

    size_t const data_qubit_end = ord.qubit_count - ord.ancilla_count;  // ancillas are [data_qubit_end, qubit_count)
    size_t gadgets_seen = 0;  // gap index i before current block

    for (size_t idx = 0; idx < middle.size(); ++idx) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&middle[idx]);
        if (cct == nullptr) {
            continue;
        }

        size_t const gap_i = gadgets_seen;
        if (gap_i > G) {
            spdlog::error(
                "validate_classical_controls_against_span_allowlist: gap index {} > G {} at block {}",
                gap_i,
                G,
                idx);
            return false;
        }

        if (cct->is_classical_control()) {
            std::unordered_set<size_t> allowed_ancillas;
            allowed_ancillas.reserve(G);
            for (size_t gid = 0; gid < G; ++gid) {
                if (ord.occupied_gadget_gap_time[gid][gap_i] == 1) {
                    allowed_ancillas.insert(gadgets[gid].ancilla_qubit);
                }
            }

            auto const ops = extract_clifford_operators(cct->operations());
            for (auto const& [type, qubits] : ops) {
                std::array<size_t, 2> used = {qubits[0], qubits[0]};
                size_t used_count = 1;
                if (type == CliffordOperatorType::cx || type == CliffordOperatorType::cz ||
                    type == CliffordOperatorType::swap || type == CliffordOperatorType::ecr) {
                    used[1] = qubits[1];
                    used_count = 2;
                }

                for (size_t k = 0; k < used_count; ++k) {
                    size_t const q = used[k];
                    if (q >= data_qubit_end && q < ord.qubit_count &&
                        allowed_ancillas.count(q) == 0) {
                        spdlog::error(
                            "sat_reorder_apply: span allowlist violation at middle[{}], gap i={}, ancilla q={} "
                            "not allowed by Span occupancy",
                            idx,
                            gap_i,
                            q);
                        return false;
                    }
                }
            }
        }

        if (cct->is_gadget()) {
            ++gadgets_seen;
        }
    }

    return true;
}

bool exported_constraint_has_ops_section(std::filesystem::path const& path) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (auto const hash = line.find('#'); hash != std::string::npos) {
            line.resize(hash);
        }
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line.erase(line.begin());
        }
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (line == "ops") {
            return true;
        }
    }
    return false;
}

}  // namespace

bool sat_reorder_export(Tableau& tableau, std::filesystem::path const& work_dir) {
    std::error_code ec;
    std::filesystem::create_directories(work_dir, ec);
    if (ec) {
        spdlog::error("sat_reorder_export: cannot create work_dir {}: {}", work_dir.string(), ec.message());
        return false;
    }

    Tableau work_tableau = tableau;
    auto const info = properize_for_degadgetization(work_tableau);
    if (!info.is_valid) {
        spdlog::error("sat_reorder_export: properize_for_degadgetization failed");
        return false;
    }

    std::filesystem::path const constraint = work_dir / "gadget_constraint.txt";
    build_constraint_graph(work_tableau, constraint.string());
    if (!std::filesystem::is_regular_file(constraint)) {
        spdlog::error("sat_reorder_export: expected constraint file missing {}", constraint.string());
        return false;
    }
    if (!exported_constraint_has_ops_section(constraint)) {
        spdlog::error(
            "sat_reorder_export: exported constraint file '{}' missing required 'ops' section",
            constraint.string());
        return false;
    }

    // Also export a stable SMT input snapshot for debugging/reruns.
    {
        std::filesystem::path const export_input_dir("/home/ferayer/TODD/run_qsyn/input");
        std::filesystem::path const export_input = export_input_dir / "gadget_constraint.txt";
        std::error_code ec2;
        std::filesystem::create_directories(export_input_dir, ec2);
        if (ec2) {
            spdlog::error(
                "sat_reorder_export: failed to create input export dir {}: {}",
                export_input_dir.string(),
                ec2.message());
            return false;
        }
        std::filesystem::copy_file(constraint, export_input, std::filesystem::copy_options::overwrite_existing, ec2);
        if (ec2) {
            spdlog::error(
                "sat_reorder_export: failed to export input {} -> {}: {}",
                constraint.string(),
                export_input.string(),
                ec2.message());
            return false;
        }
        spdlog::info("sat_reorder_export: exported input to {}", export_input.string());
    }
    spdlog::info("sat_reorder_export: wrote {}", constraint.string());
    return true;
}

bool sat_reorder_run_solver(std::filesystem::path const& work_dir, std::filesystem::path const& sat_formulation_py) {
    if (!std::filesystem::is_regular_file(sat_formulation_py)) {
        spdlog::error("sat_reorder_run_solver: missing script {}", sat_formulation_py.string());
        return false;
    }
    std::filesystem::path const input = work_dir / "gadget_constraint.txt";
    std::filesystem::path const ordering_out = work_dir / "gadget_ordering.txt";
    std::filesystem::path const export_input_dir("/home/ferayer/TODD/run_qsyn/input");
    std::filesystem::path const export_output_dir("/home/ferayer/TODD/run_qsyn/output");
    if (!std::filesystem::is_regular_file(input)) {
        spdlog::error("sat_reorder_run_solver: missing {}", input.string());
        return false;
    }
    {
        std::error_code ec;
        std::filesystem::create_directories(export_input_dir, ec);
        if (ec) {
            spdlog::error(
                "sat_reorder_run_solver: failed to create input export dir {}: {}",
                export_input_dir.string(),
                ec.message());
        } else {
            std::filesystem::path const export_input = export_input_dir / input.filename();
            std::filesystem::copy_file(input, export_input, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                spdlog::error(
                    "sat_reorder_run_solver: failed to export input {} -> {}: {}",
                    input.string(),
                    export_input.string(),
                    ec.message());
            } else {
                spdlog::info("sat_reorder_run_solver: exported input to {}", export_input.string());
            }
        }
    }

    std::ostringstream cmd;
    cmd << "python3 " << shell_single_quote(sat_formulation_py) << " --input " << shell_single_quote(input)
        << " --ordering-out " << shell_single_quote(ordering_out) << " --quiet";

    std::string const cmd_str = cmd.str();
    spdlog::info("sat_reorder_run_solver: {}", cmd_str);
    int const rc = std::system(cmd_str.c_str());
    if (rc != 0) {
        spdlog::warn("sat_reorder_run_solver: exit code {}", rc);
        return false;
    }
    if (!std::filesystem::is_regular_file(ordering_out)) {
        spdlog::warn("sat_reorder_run_solver: expected output missing {}", ordering_out.string());
        return false;
    }
    {
        std::error_code ec;
        std::filesystem::create_directories(export_output_dir, ec);
        if (ec) {
            spdlog::error(
                "sat_reorder_run_solver: failed to create output export dir {}: {}",
                export_output_dir.string(),
                ec.message());
        } else {
            std::filesystem::path const export_output = export_output_dir / ordering_out.filename();
            std::filesystem::copy_file(ordering_out, export_output, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                spdlog::error(
                    "sat_reorder_run_solver: failed to export output {} -> {}: {}",
                    ordering_out.string(),
                    export_output.string(),
                    ec.message());
            } else {
                spdlog::info("sat_reorder_run_solver: exported output to {}", export_output.string());
            }
        }
    }
    return true;
}

bool sat_reorder_apply(Tableau& tableau, std::filesystem::path const& ordering_path) {
    std::string perr;
    ParsedGadgetOrdering ord;
    if (!parse_gadget_ordering_file(ordering_path, ord, perr)) {
        spdlog::error("sat_reorder_apply: parse failed: {}", perr);
        return false;
    }

    auto const info = properize_for_degadgetization(tableau);
    if (!info.is_valid) {
        spdlog::error("sat_reorder_apply: properize_for_degadgetization failed");
        return false;
    }

    auto gadgets = export_hadamard_gadget_pairs(tableau);
    size_t const num_gadgets = gadgets.size();
    std::string verr;
    if (!validate_ordering_against_tableau(tableau, gadgets, ord, verr)) {
        spdlog::error("sat_reorder_apply: {}", verr);
        return false;
    }
    if (ord.span_by_gid.empty()) {
        spdlog::error(
            "sat_reorder_apply: ordering file must include a non-empty Span section (PMC placement); {}",
            ordering_path.string());
        return false;
    }

    log_and_export_spans(ordering_path, ord);

    std::unordered_map<size_t, ConstraintGraph::PRInfo> pr_vertex_to_info;
    size_t global_pr_counter = 0;
    for (size_t idx = 0; idx < tableau.size(); ++idx) {
        auto const* pr_vec = std::get_if<std::vector<PauliRotation>>(&tableau[idx]);
        if (!pr_vec) {
            continue;
        }
        for (size_t pr_idx = 0; pr_idx < pr_vec->size(); ++pr_idx) {
            size_t const vertex_id = num_gadgets + global_pr_counter;
            pr_vertex_to_info[vertex_id] = {idx, pr_idx, global_pr_counter, &(*pr_vec)[pr_idx]};
            ++global_pr_counter;
        }
    }

    std::unordered_map<size_t, std::vector<PauliRotation>> pr_columns;
    for (size_t vid = num_gadgets; vid < num_gadgets + global_pr_counter; ++vid) {
        auto it = pr_vertex_to_info.find(vid);
        if (it == pr_vertex_to_info.end()) {
            continue;
        }
        auto const& pr_info = it->second;
        auto const* pr_vec = std::get_if<std::vector<PauliRotation>>(&tableau[pr_info.tableau_index]);
        if (pr_vec && pr_info.pr_vector_index < pr_vec->size()) {
            pr_columns[vid] = {(*pr_vec)[pr_info.pr_vector_index]};
        }
    }

    for (auto const& [pid, gap] : ord.column_slot) {
        (void)gap;
        if (!pr_columns.count(pid)) {
            spdlog::error("sat_reorder_apply: column_slot pid {} has no PR column in tableau", pid);
            return false;
        }
    }

    size_t const G = num_gadgets;
    std::vector<std::vector<size_t>> paulis_at_gap(G + 1);
    for (auto const& [pid, gap] : ord.column_slot) {
        paulis_at_gap[gap].push_back(pid);
    }
    for (auto& bucket : paulis_at_gap) {
        std::sort(bucket.begin(), bucket.end());
    }

    if (tableau.size() < 2) {
        spdlog::error("sat_reorder_apply: tableau too small");
        return false;
    }

    std::vector<ClassicalControlTableau> pmc_for_gid;
    pmc_for_gid.reserve(G);
    for (size_t gid = 0; gid < G; ++gid) {
        auto* pmc_ptr = std::get_if<ClassicalControlTableau>(&tableau[gadgets[gid].pmc_index]);
        if (!pmc_ptr || !pmc_ptr->is_classical_control()) {
            spdlog::error("sat_reorder_apply: missing classical-control PMC for gid {}", gid);
            return false;
        }
        pmc_for_gid.push_back(*pmc_ptr);
    }

    std::deque<ClassicalControlTableau> temp_cccs;
    for (size_t r = 0; r < G; ++r) {
        size_t const gid = ord.gadget_order_gids[r];
        size_t const ccc_idx = gadgets[gid].ccc_index;
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[ccc_idx]);
        if (cct && cct->is_gadget()) {
            temp_cccs.push_back(*cct);
        }
    }

    std::vector<SubTableau> middle;
    middle.reserve(tableau.size() + G + global_pr_counter);

    auto emit_pr_column = [&](size_t vertex_id) -> bool {
        auto pit = pr_columns.find(vertex_id);
        if (pit == pr_columns.end()) {
            spdlog::error("sat_reorder_apply: missing PR column for vertex {}", vertex_id);
            return false;
        }
        std::vector<PauliRotation> pr_column = std::move(pit->second);
        pr_columns.erase(pit);
        for (auto it = temp_cccs.rbegin(); it != temp_cccs.rend(); ++it) {
            auto& ccc = *it;
            size_t const a = ccc.reference_qubit();
            size_t const b = ccc.ancilla_qubit();
            for (auto& rotation : pr_column) {
                swap_gadget_phase_slots(rotation, a, b);
            }
        }
        middle.push_back(std::move(pr_column));
        return true;
    };

    for (size_t g = 0; g < G; ++g) {
        for (size_t vid : paulis_at_gap[g]) {
            if (!emit_pr_column(vid)) {
                return false;
            }
        }
        if (temp_cccs.empty()) {
            spdlog::error("sat_reorder_apply: gadget emission but temp_cccs empty");
            return false;
        }
        middle.push_back(std::move(temp_cccs.front()));
        temp_cccs.pop_front();
    }

    for (size_t vid : paulis_at_gap[G]) {
        if (!emit_pr_column(vid)) {
            return false;
        }
    }
    for (size_t gid = 0; gid < G; ++gid) {
        middle.push_back(std::move(pmc_for_gid[gid]));
    }
    if (!permute_pmcs_in_middle_for_sat_order(middle, G, ord, gadgets)) {
        return false;
    }

    if (!validate_classical_controls_against_span_allowlist(middle, G, ord, gadgets)) {
        return false;
    }

    if (!temp_cccs.empty()) {
        spdlog::warn("sat_reorder_apply: {} unconsumed gadget(s) in queue (should be empty)", temp_cccs.size());
    }

    auto const back_it = tableau.end() - 1;
    tableau.erase(tableau.begin() + 1, back_it);
    for (auto it = middle.begin(); it != middle.end(); ++it) {
        tableau.insert(tableau.end() - 1, std::move(*it));
    }

    reestablish_hadamard_gadget_pairing(tableau);
    remove_identities(tableau);
    spdlog::info("sat_reorder_apply: done ({} elements)", tableau.size());
    return true;
}

void sat_reorder(Tableau& tableau) {
    int const pid =
#ifdef __unix__
        static_cast<int>(getpid());
#else
        0;
#endif
    std::filesystem::path const work_dir =
        std::filesystem::temp_directory_path() / fmt::format("qsyn_sat_{}", pid);

    std::error_code ec;
    std::filesystem::create_directories(work_dir, ec);
    if (ec) {
        spdlog::error("sat_reorder: mkdir {} failed: {}", work_dir.string(), ec.message());
        return;
    }

    std::filesystem::path const script = resolve_sat_formulation_script();
    if (script.empty()) {
        spdlog::warn(
            "sat_reorder: set QSYN_SAT_FORMULATION to sat_formulation.py or run from a directory "
            "that contains ancilla-minimization-with-sat/; falling back to topological reorder");
        return;
    }

    if (!sat_reorder_export(tableau, work_dir)) {
        return;
    }
    if (!sat_reorder_run_solver(work_dir, script)) {
        spdlog::error("sat_reorder: SAT run failed");
        spdlog::warn("sat_reorder: SAT run failed");
        return;
    }
    std::filesystem::path const ordering = work_dir / "gadget_ordering.txt";
    if (!sat_reorder_apply(tableau, ordering)) {
        spdlog::error("sat_reorder: apply failed");
        spdlog::warn("sat_reorder: apply failed");
    }
}

}  // namespace qsyn::experimental
