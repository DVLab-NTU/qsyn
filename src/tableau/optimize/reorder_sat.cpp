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
#include <cstdlib>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef __unix__
#include <unistd.h>
#endif

namespace qsyn::experimental {

namespace {

void swap_gadget_phase_slots(PauliRotation& r, size_t reference, size_t ancilla) {
    if (reference == ancilla) {
        return;
    }
    if (!r.is_diagonal()) {
        return;
    }

    std::vector<Pauli> pv(r.n_qubits(), Pauli::i);
    for (size_t q = 0; q < r.n_qubits(); ++q) {
        pv[q] = r.get_pauli_type(q);
    }
    std::swap(pv[reference], pv[ancilla]);
    dvlab::Phase const ph = r.phase();
    r                    = PauliRotation(pv.begin(), pv.end(), ph);

    if (r.phase() != dvlab::Phase(0)) {
        r.set_is_CZ(false);
        return;
    }
    size_t z_count = 0;
    for (size_t q = 0; q < r.n_qubits(); ++q) {
        if (r.get_pauli_type(q) == Pauli::z) ++z_count;
    }
    r.set_is_CZ(z_count == 2);
}

enum class ScheduleParseSection { None, GadgetOrder, ColumnSlot, Span };

struct ParsedGadgetOrdering {
    size_t qubit_count  = 0;
    size_t ancilla_count = 0;
    std::vector<size_t> gadget_order_gids;
    std::unordered_map<size_t, size_t> column_slot;
    /// From ``Span :`` section: gadget id -> (min_i, max_i) gap indices.
    std::unordered_map<size_t, std::pair<size_t, size_t>> span_by_gid;
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
            std::istringstream iss(line);
            size_t gid = 0;
            size_t min_i = 0;
            size_t max_i = 0;
            if (!(iss >> gid >> min_i >> max_i)) {
                err = fmt::format("bad Span line: {}", line);
                return false;
            }
            if (out.span_by_gid.count(gid) != 0) {
                err = fmt::format("duplicate Span gid {}", gid);
                return false;
            }
            out.span_by_gid.emplace(gid, std::pair<size_t, size_t>{min_i, max_i});
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

enum class LinearOpKind { Pauli, Gadget };

struct LinearOp {
    LinearOpKind kind;
    size_t vertex_id;
};

}  // namespace

bool sat_reorder_export(Tableau& tableau, std::filesystem::path const& work_dir) {
    std::error_code ec;
    std::filesystem::create_directories(work_dir, ec);
    if (ec) {
        spdlog::error("sat_reorder_export: cannot create work_dir {}: {}", work_dir.string(), ec.message());
        return false;
    }

    auto const info = properize_for_degadgetization(tableau);
    if (!info.is_valid) {
        spdlog::error("sat_reorder_export: properize_for_degadgetization failed");
        return false;
    }

    std::filesystem::path const constraint = work_dir / "gadget_constraint.txt";
    build_constraint_graph(tableau, constraint.string());
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
    if (!std::filesystem::is_regular_file(input)) {
        spdlog::error("sat_reorder_run_solver: missing {}", input.string());
        return false;
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

    std::vector<LinearOp> linear;
    for (size_t g = 0; g < G; ++g) {
        for (size_t vid : paulis_at_gap[g]) {
            linear.push_back({LinearOpKind::Pauli, vid});
        }
        linear.push_back({LinearOpKind::Gadget, 0});
    }
    for (size_t vid : paulis_at_gap[G]) {
        linear.push_back({LinearOpKind::Pauli, vid});
    }

    StabilizerTableau front_st = *std::get_if<StabilizerTableau>(&tableau[0]);
    StabilizerTableau back_st  = *std::get_if<StabilizerTableau>(&tableau[tableau.size() - 1]);

    std::vector<ClassicalControlTableau> pmcs;
    for (size_t idx = 0; idx < tableau.size(); ++idx) {
        auto const* cct = std::get_if<ClassicalControlTableau>(&tableau[idx]);
        if (cct && cct->is_classical_control()) {
            pmcs.push_back(*cct);
        }
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

    Tableau new_tableau{tableau.n_qubits()};
    new_tableau.set_n_ancilla(tableau.n_ancilla());
    if (!tableau.is_empty()) {
        tableau.erase(tableau.begin(), tableau.end());
    }
    new_tableau.push_back(std::move(front_st));

    for (LinearOp const& op : linear) {
        if (op.kind == LinearOpKind::Gadget) {
            if (temp_cccs.empty()) {
                spdlog::error("sat_reorder_apply: gadget emission but temp_cccs empty");
                return false;
            }
            new_tableau.push_back(std::move(temp_cccs.front()));
            temp_cccs.pop_front();
        } else {
            size_t const vertex_id = op.vertex_id;
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
            new_tableau.push_back(std::move(pr_column));
        }
    }

    if (!temp_cccs.empty()) {
        spdlog::warn("sat_reorder_apply: {} unconsumed gadget(s) in queue (should be empty)", temp_cccs.size());
    }

    for (auto& pmc : pmcs) {
        new_tableau.push_back(std::move(pmc));
    }
    new_tableau.push_back(std::move(back_st));

    reestablish_hadamard_gadget_pairing(new_tableau);

    tableau = std::move(new_tableau);
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
        reorder_n_degadgetize(tableau);
        return;
    }

    std::filesystem::path const script = resolve_sat_formulation_script();
    if (script.empty()) {
        spdlog::warn(
            "sat_reorder: set QSYN_SAT_FORMULATION to sat_formulation.py or run from a directory "
            "that contains ancilla-minimization-with-sat/; falling back to topological reorder");
        reorder_n_degadgetize(tableau);
        return;
    }

    if (!sat_reorder_export(tableau, work_dir)) {
        reorder_n_degadgetize(tableau);
        return;
    }
    if (!sat_reorder_run_solver(work_dir, script)) {
        spdlog::warn("sat_reorder: SAT run failed; falling back to reorder_n_degadgetize");
        reorder_n_degadgetize(tableau);
        return;
    }
    std::filesystem::path const ordering = work_dir / "gadget_ordering.txt";
    if (!sat_reorder_apply(tableau, ordering)) {
        spdlog::warn("sat_reorder: apply failed; falling back to reorder_n_degadgetize");
        reorder_n_degadgetize(tableau);
    }
}

}  // namespace qsyn::experimental
