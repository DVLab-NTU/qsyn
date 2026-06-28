/**
 * @file
 * @brief FastTODD phase polynomial optimization (TOHPE + fast_todd_iteration)
 * @copyright Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan
 */

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <random>
#include <ranges>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../tableau_optimization.hpp"
#include "./todd.hpp"
#include "fmt/core.h"
#include "tableau/pauli_rotation.hpp"
#include "tableau/stabilizer_tableau.hpp"
#include "util/boolean_matrix.hpp"
#include "util/phase.hpp"
#include "util/util.hpp"

extern bool stop_requested();

namespace qsyn::experimental {
using namespace todd;
using namespace signature;
using namespace qsyn::experimental;

namespace {

constexpr size_t k_bit_block_size = 256;

size_t block_bit_width(size_t logical_bits) {
    return ((logical_bits + k_bit_block_size - 1) / k_bit_block_size) * k_bit_block_size;
}

void pad_row_to_width(dvlab::BooleanMatrix::Row& row, size_t width) {
    if (row.size() >= width) {
        return;
    }
    auto bits = row.get_row();
    bits.resize(width, 0);
    row.set_row(std::move(bits));
}

/**
 * @brief Transform matrix to reduced-row-echelon form, store row operations in augmented_matrix, and get it pivots.
 *
 * @param matrix, augmented_matrix, pivots
 * @return null
 */
dvlab::BooleanMatrix::Row kernel(
    dvlab::BooleanMatrix&               matrix,
    dvlab::BooleanMatrix&               augmented_matrix,
    std::unordered_map<size_t, size_t>& pivots) {
    for (auto i : std::views::iota(0ul, matrix.num_rows())) {
        if (pivots.contains(i)) continue;
        std::vector<size_t> pivot_keys;
        pivot_keys.reserve(pivots.size());
        for (auto const& [key, _] : pivots) {
            pivot_keys.push_back(key);
        }
        std::sort(pivot_keys.begin(), pivot_keys.end());
        for (auto const key : pivot_keys) {
            auto const value = pivots.at(key);
            if (matrix[i][value]) {
                matrix[i] += matrix[key];
                augmented_matrix[i] += augmented_matrix[key];
            }
        }

        size_t const first_one_idx =
            std::distance(matrix[i].begin(), std::find(matrix[i].begin(), matrix[i].end(), true));
        if (!matrix[i][first_one_idx]) {
            return augmented_matrix[i];
        }

        auto pivot           = matrix[i];
        auto augmented_pivot = augmented_matrix[i];
        pivot_keys.clear();
        pivot_keys.reserve(pivots.size());
        for (auto const& [key, _] : pivots) {
            pivot_keys.push_back(key);
        }
        std::sort(pivot_keys.begin(), pivot_keys.end());
        for (auto const key : pivot_keys) {
            if (matrix[key][first_one_idx]) {
                matrix[key] += pivot;
                augmented_matrix[key] += augmented_pivot;
            }
        }
        pivots[i] = first_one_idx;
    }
    return dvlab::BooleanMatrix::Row(0);
}

/**
 * @brief Transform matrix to reduced-row-echelon form, store row operations in augmented_matrix, and get it pivots.
 *
 * @param matrix, augmented_matrix, pivots
 * @return null
 */
dvlab::BooleanMatrix get_l_matrix(dvlab::BooleanMatrix const& phase_poly_matrix, dvlab::BooleanMatrix const& row_products) {
    auto l_matrix       = phase_poly_matrix;  // copy
    auto const num_rows = phase_poly_matrix.num_rows();

    // [Note] the index here is different from the index of original repository impl
    auto const get_row_product_idx = [&num_rows](size_t a, size_t b) {
        if (a > b) {
            std::swap(a, b);
        }
        return (a * num_rows) - (a * (a + 1) / 2) + b - a - 1;
    };

    auto const id_vec = std::views::iota(0ul, num_rows) | tl::to<std::vector>();

    for (auto const& [a, b] : dvlab::combinations<2>(id_vec)) {
        auto const new_row = row_products[get_row_product_idx(a, b)];
        l_matrix.push_row(new_row);
    }

    return l_matrix;
}

[[maybe_unused]] int calculate_score(dvlab::BooleanMatrix::Row const& y, std::vector<std::pair<int, int>> const& s_matrix) {
    const int abs_y = static_cast<int>(y.sum() % 2);
    int ret         = -1 * abs_y;

    for (auto& indexes : s_matrix) {
        if (indexes.first == indexes.second) {
            ret += y[indexes.first] + 2 * (y[indexes.first] == 0) * (abs_y);
        } else {
            ret += 2 * (y[indexes.first] ^ y[indexes.second]);
        }
    }
    return ret;
}

bool compare_row_value(dvlab::BooleanMatrix::Row const& a, dvlab::BooleanMatrix::Row const& b) {
    DVLAB_ASSERT(a.size() == b.size(), "Row compare should have same size.");
    for (auto i : std::views::iota(0ul, a.size())) {
        if (!a[i] ^ b[i]) continue;
        return a[i] < b[i];
    }
    // a == b
    return false;
}

void clear_column(size_t idx, dvlab::BooleanMatrix& matrix, dvlab::BooleanMatrix& augmented_matrix, std::unordered_map<size_t, size_t>& pivots) {
    if (!pivots.contains(idx)) return;
    auto val = pivots[idx];
    pivots.erase(idx);

    if (!augmented_matrix[idx][idx]) {
        for (auto j : std::views::iota(0ul, matrix.num_rows())) {
            if (!augmented_matrix[j][idx]) {
                continue;
            }
            pivots[j]             = val;
            auto col              = matrix[j];
            auto augmented_col    = augmented_matrix[j];
            matrix[j]             = matrix[idx];
            augmented_matrix[j]   = augmented_matrix[idx];
            matrix[idx]           = col;
            augmented_matrix[idx] = augmented_col;
            break;
        }
    }

    auto col           = matrix[idx];
    auto augmented_col = augmented_matrix[idx];
    for (auto j : std::views::iota(0ul, matrix.num_rows())) {
        if (augmented_matrix[j][idx] && idx != j) {
            matrix[j] += col;
            augmented_matrix[j] += augmented_col;
        }
    }
}

using IntegerVec = std::vector<__int128>;

bool integer_vec_less(IntegerVec const& a, IntegerVec const& b) {
    size_t const n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return a[i] < b[i];
        }
    }
    return a.size() < b.size();
}

struct IntegerVecHash {
    size_t operator()(IntegerVec const& v) const noexcept {
        size_t h = v.size();
        for (auto const x : v) {
            auto const lo = static_cast<unsigned long long>(x);
            auto const hi = static_cast<unsigned long long>(static_cast<__uint128_t>(x) >> 64);
            h ^= std::hash<unsigned long long>{}(lo) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<unsigned long long>{}(hi);
        }
        return h;
    }
};

/** Pack term row bits for TODD-style bit-vector integer encoding. */
IntegerVec row_to_integer_vec(dvlab::BooleanMatrix::Row const& row) {
    constexpr size_t BLOCK_SIZE = 256;
    size_t const     num_blocks = std::max<size_t>(1, (row.size() + BLOCK_SIZE - 1) / BLOCK_SIZE);
    std::vector<std::array<int32_t, 8>> blocks(num_blocks);
    for (size_t bit = 0; bit < row.size(); ++bit) {
        if (!row[bit]) {
            continue;
        }
        size_t const block_index  = bit / BLOCK_SIZE;
        size_t const bit_in_block = bit % BLOCK_SIZE;
        size_t const lane_index   = bit_in_block / 32;
        size_t const bit_in_lane  = bit_in_block % 32;
        blocks[block_index][lane_index] ^= (1 << bit_in_lane);
    }
    IntegerVec result;
    result.reserve(num_blocks * 2);
    for (auto const& arr : blocks) {
        for (size_t k = 0; k < 2; ++k) {
            __int128 integer = 0;
            for (size_t j = 0; j < 4; ++j) {
                auto const val = static_cast<uint32_t>(arr[k * 4 + j]);
                integer ^= static_cast<__int128>(val) << (32 * j);
            }
            result.push_back(integer);
        }
    }
    return result;
}

bool fasttodd_trace_enabled() {
    static int const enabled = [] {
        if (char const* v = std::getenv("QSYN_FASTTODD_TRACE")) {
            return (v[0] == '1' || v[0] == 'y' || v[0] == 'Y') ? 1 : 0;
        }
        return 0;
    }();
    return enabled != 0;
}

bool env_enabled(char const* value) {
    return value != nullptr && (value[0] == '1' || value[0] == 'y' || value[0] == 'Y');
}

bool fasttodd_trace_ties_enabled() {
    static int const enabled = [] {
        if (char const* v = std::getenv("QSYN_FASTTODD_TRACE_TIES")) {
            return env_enabled(v) ? 1 : 0;
        }
        return 0;
    }();
    return enabled != 0;
}

bool fasttodd_random_tie_break_enabled() {
    static int const enabled = [] {
        if (char const* v = std::getenv("QSYN_FASTTODD_RANDOM_TIE_BREAK")) {
            return env_enabled(v) ? 1 : 0;
        }
        return 0;
    }();
    return enabled != 0;
}

std::mt19937_64& fasttodd_rng() {
    static std::mt19937_64 rng = [] {
        if (char const* v = std::getenv("QSYN_FASTTODD_RANDOM_SEED")) {
            try {
                return std::mt19937_64{static_cast<std::uint64_t>(std::stoull(v))};
            } catch (...) {
                spdlog::warn(
                    "QSYN_FASTTODD_RANDOM_SEED='{}' is invalid; using std::random_device", v);
            }
        }
        return std::mt19937_64{std::random_device{}()};
    }();
    return rng;
}

std::optional<FastToddTieControl>   g_fasttodd_tie_control;
std::optional<FastToddTieRunReport> g_fasttodd_tie_run_report;

struct FastToddTieRuntime {
    bool enabled = false;
    FastToddTieSearchMode mode = FastToddTieSearchMode::random_target_only;
    std::optional<FastToddTieStepTarget> target_random_step = std::nullopt;
    std::unordered_map<size_t, size_t>   forced_tohpe_choice_by_step;
    std::unordered_map<size_t, size_t>   forced_outer_choice_by_step;
    std::mt19937_64                      rng{0};
    FastToddTieRunReport                 report;
};

size_t choose_controlled_tie_index(FastToddTieRuntime& runtime,
                                   FastToddTieLevel const level,
                                   size_t const step_index,
                                   size_t const tie_count) {
    if (tie_count == 0) {
        return 0;
    }

    auto const& forced = level == FastToddTieLevel::tohpe
        ? runtime.forced_tohpe_choice_by_step
        : runtime.forced_outer_choice_by_step;
    if (auto const it = forced.find(step_index); it != forced.end() && it->second < tie_count) {
        return it->second;
    }
    if ((runtime.mode == FastToddTieSearchMode::random_target_only ||
         runtime.mode == FastToddTieSearchMode::force_prefix_random_target) &&
        runtime.target_random_step.has_value() &&
        runtime.target_random_step->level == level &&
        runtime.target_random_step->step_index == step_index &&
        tie_count > 1) {
        std::uniform_int_distribution<size_t> dist(0, tie_count - 1);
        return dist(runtime.rng);
    }
    if (runtime.mode == FastToddTieSearchMode::all_random && tie_count > 1) {
        std::uniform_int_distribution<size_t> dist(0, tie_count - 1);
        return dist(runtime.rng);
    }
    return 0;
}

void record_controlled_decision(FastToddTieRuntime& runtime,
                                FastToddTieLevel const level,
                                size_t const step_index,
                                size_t const tie_count,
                                size_t const chosen_index) {
    FastToddTieStepDecision const decision{
        .step_index = step_index,
        .tie_count = tie_count,
        .chosen_index = chosen_index,
    };
    if (level == FastToddTieLevel::tohpe) {
        runtime.report.tohpe_decisions.push_back(decision);
    } else {
        runtime.report.outer_decisions.push_back(decision);
    }
    if (fasttodd_trace_ties_enabled()) {
        fmt::print(
            stderr,
            "[cpp-fasttodd] level={} step={} tie_count={} chosen_index={}\n",
            level == FastToddTieLevel::tohpe ? "tohpe" : "outer",
            step_index,
            tie_count,
            chosen_index);
    }
}

std::string row_bits_string(dvlab::BooleanMatrix::Row const& row) {
    std::string s;
    s.reserve(row.size());
    for (auto b : row) {
        s.push_back(b ? '1' : '0');
    }
    return s;
}

void trace_dump_table(char const* tag, char const* backend, dvlab::BooleanMatrix const& table) {
    if (!fasttodd_trace_enabled()) {
        return;
    }
    fmt::print(stderr, "[{}-fasttodd] {} rows={}\n", backend, tag, table.num_rows());
    for (size_t i = 0; i < table.num_rows(); ++i) {
        fmt::print(stderr, "[{}-fasttodd]   term[{}] {}\n", backend, i, row_bits_string(table[i]));
    }
}

void trace_chosen_move(char const* backend, size_t outer, size_t i, size_t j, int score,
                       dvlab::BooleanMatrix::Row const& z, dvlab::BooleanMatrix::Row const& y,
                       size_t rows_after) {
    if (!fasttodd_trace_enabled()) {
        return;
    }
    fmt::print(stderr, "[{}-fasttodd] outer={} pair=({},{}) score={} z={} y={} rows_after={}\n",
               backend, outer, i, j, score, row_bits_string(z), row_bits_string(y), rows_after);
}

/** Extended row for FastTODD/TOHPE: linear Z bits + pop-extend quadratic block. */
dvlab::BooleanMatrix::Row build_extended_row_todd(dvlab::BooleanMatrix::Row const& term_z, size_t n_qubits) {
    std::vector<unsigned char> row;
    row.reserve(n_qubits + (n_qubits * (n_qubits - 1)) / 2);
    for (size_t q = 0; q < n_qubits; ++q) {
        row.push_back((q < term_z.size()) ? term_z[q] : 0);
    }
    std::vector<unsigned char> t_vec(row.begin(), row.begin() + static_cast<std::ptrdiff_t>(n_qubits));
    std::vector<unsigned char> ext;
    for (size_t iter = 0; iter < n_qubits; ++iter) {
        if (t_vec.empty()) {
            break;
        }
        unsigned char const popped = t_vec.back();
        t_vec.pop_back();
        if (popped) {
            ext.insert(ext.end(), t_vec.begin(), t_vec.end());
        } else {
            ext.insert(ext.end(), t_vec.size(), 0);
        }
    }
    row.insert(row.end(), ext.begin(), ext.end());
    return dvlab::BooleanMatrix::Row(std::move(row));
}

/** Build one row of transpose(L) from a term, matching C++ get_row_products/get_l_matrix column order. */
dvlab::BooleanMatrix::Row build_l_transpose_row_from_term(dvlab::BooleanMatrix::Row const& term_z, size_t n_qubits) {
    DVLAB_ASSERT(term_z.size() >= n_qubits, "term row shorter than n_qubits");
    std::vector<unsigned char> vec{};
    vec.reserve(n_qubits + (n_qubits * (n_qubits - 1) / 2));
    for (size_t q = 0; q < n_qubits; ++q) {
        vec.emplace_back(term_z[q]);
    }
    for (size_t a = 0; a < n_qubits; ++a) {
        for (size_t b = a + 1; b < n_qubits; ++b) {
            vec.emplace_back(static_cast<unsigned char>(term_z[a] & term_z[b]));
        }
    }
    return dvlab::BooleanMatrix::Row(vec);
}

dvlab::BooleanMatrix get_z_matrix(dvlab::BooleanMatrix const& phase_poly_matrix) {
    auto z_matrix_transposed = dvlab::transpose(phase_poly_matrix);
    auto const num_terms     = phase_poly_matrix.num_cols();
    auto const n_qubits      = phase_poly_matrix.num_rows();

    // Enumerate pairs of existing phase-polynomial terms. Using the pair count here
    // produces synthetic indices past the last real column and can segfault.
    auto const id_vec = std::views::iota(0ul, num_terms) | tl::to<std::vector>();
    auto seen_z       = std::unordered_set<dvlab::BooleanMatrix::Row, dvlab::BooleanMatrixRowHash>();

    if (num_terms < 2) {
        return z_matrix_transposed;
    }

    for (auto const& [a, b] : dvlab::combinations<2>(id_vec)) {
        if (stop_requested()) {
            return phase_poly_matrix;
        }
        dvlab::BooleanMatrix::Row z(n_qubits);
        for (size_t k = 0; k < n_qubits; ++k) {
            z[k] = phase_poly_matrix[k][a] ^ phase_poly_matrix[k][b];
        }

        if (seen_z.contains(z)) {
            continue;
        }
        seen_z.insert(z);

        z_matrix_transposed.push_row(z);
    }
    return z_matrix_transposed;
}

std::vector<std::vector<std::pair<int, int>>> get_s_matrices(dvlab::BooleanMatrix const& phase_poly_matrix, dvlab::BooleanMatrix const& z_matrix) {
    std::vector<std::vector<std::pair<int, int>>> s{};
    auto phase_poly_matrix_transposed = dvlab::transpose(phase_poly_matrix);
    auto const n_qubits               = phase_poly_matrix.num_rows();
    auto const num_terms              = phase_poly_matrix.num_cols();
    auto const id_vec                 = std::views::iota(0ul, num_terms) | tl::to<std::vector>();

    for (auto& z : z_matrix.get_matrix()) {
        std::vector<std::pair<int, int>> s_z;
        // a != b
        for (auto const& [a, b] : dvlab::combinations<2>(id_vec)) {
            dvlab::BooleanMatrix::Row tmp(n_qubits);
            for (size_t k = 0; k < n_qubits; ++k) {
                tmp[k] = phase_poly_matrix[k][a] ^ phase_poly_matrix[k][b];
            }
            if (tmp == z) {
                s_z.push_back(std::make_pair(a, b));
            }
        }
        // a == b
        for (auto a : std::views::iota(0ul, num_terms)) {
            if (phase_poly_matrix_transposed.get_row(a) == z) {
                s_z.push_back(std::make_pair(a, a));
            }
        }
        s.push_back(s_z);
    }

    return s;
}

Polynomial tohpe_once(Polynomial const& polynomial) {
    if (polynomial.empty()) {
        return polynomial;
    }

    // auto const n_qubits = polynomial.front().n_qubits();

    // Each column represents a term in the phase polynomial, and each row represents a qubit.
    auto const phase_poly_matrix = load_phase_poly_matrix(polynomial);

    auto const idx_vec = std::views::iota(0ul, polynomial.size()) | tl::to<std::vector>();

    auto const row_products = get_row_products(phase_poly_matrix);

    auto const l_matrix             = get_l_matrix(phase_poly_matrix, row_products);
    auto const z_matrix             = get_z_matrix(phase_poly_matrix);
    auto const s_matrices           = get_s_matrices(phase_poly_matrix, z_matrix);
    auto const nullspace_transposed = get_nullspace_transposed(l_matrix);

    if (nullspace_transposed.is_empty()) {
        return polynomial;
    }

    auto phase_poly_matrix_copy = phase_poly_matrix;
    for (auto const& y : nullspace_transposed) {
        if (y.is_zeros()) {
            continue;
        } else if (y.sum() != y.size() && y.sum() % 2 == 0) {
            // y is candidate

            int max_score    = std::numeric_limits<int>::min();
            size_t max_index = 0;
            for (auto a : std::views::iota(0ul, s_matrices.size())) {
                auto const& s_matrix = s_matrices[a];
                auto score           = calculate_score(y, s_matrix);
                if (score > max_score) {
                    max_score = score;
                    max_index = a;
                }
            }

            auto& chosen_z = z_matrix[max_index];

            for (auto const i : std::views::iota(0ul, phase_poly_matrix_copy.num_rows())) {
                if (chosen_z[i] == 1) {
                    phase_poly_matrix_copy[i] += y;
                }
            }

            phase_poly_matrix_copy = dvlab::transpose(phase_poly_matrix_copy);
            if (y.sum() % 2 == 1) {
                phase_poly_matrix_copy.push_row(chosen_z);
            }
            return from_boolean_matrix(phase_poly_matrix_copy);
        }
    }
    return from_boolean_matrix(dvlab::transpose(phase_poly_matrix_copy));
}

/** Full TOHPE pass on term table. */
void tohpe_table_pass(dvlab::BooleanMatrix& table,
                      size_t n_qubits,
                      size_t max_moves = static_cast<size_t>(-1),
                      FastToddTieRuntime* tie_runtime = nullptr) {
    if (table.num_rows() == 0) {
        return;
    }

    dvlab::BooleanMatrix extended_matrix;
    extended_matrix.reserve(table.num_rows(), n_qubits + (n_qubits * (n_qubits - 1)) / 2);
    for (size_t i = 0; i < table.num_rows(); ++i) {
        extended_matrix.push_row(build_extended_row_todd(table[i], n_qubits));
    }

    dvlab::BooleanMatrix augmented_matrix = dvlab::identity(table.num_rows());
    std::unordered_map<size_t, size_t> pivots;

    size_t move_idx = 0;
    while (true) {
        auto n_terms                = table.num_rows();
        dvlab::BooleanMatrix::Row y = kernel(extended_matrix, augmented_matrix, pivots);
        if (y.is_zeros()) {
            break;
        }

        std::unordered_map<IntegerVec, int, IntegerVecHash>                          score_map{};
        std::unordered_map<IntegerVec, dvlab::BooleanMatrix::Row, IntegerVecHash> z_map{};
        auto const y_parity = y.sum() % 2;
        for (auto i : std::views::iota(0ul, n_terms)) {
            if (y[i] != y_parity) {
                auto const key = row_to_integer_vec(table[i]);
                score_map.try_emplace(key, 1);
                z_map.try_emplace(key, table[i]);
            }
        }

        for (auto i : std::views::iota(0ul, n_terms)) {
            if (!y[i]) continue;
            for (auto j : std::views::iota(0ul, n_terms)) {
                if (y[j]) continue;
                auto const z   = table[i] + table[j];
                auto const key = row_to_integer_vec(z);
                auto [it, inserted] = score_map.try_emplace(key, 2);
                if (inserted) {
                    z_map.try_emplace(key, z);
                } else {
                    it->second += 2;
                    z_map.find(key)->second = z;
                }
            }
        }

        int max_score = 0;
        std::vector<std::pair<IntegerVec, dvlab::BooleanMatrix::Row>> tied_max{};
        for (auto const& [key, score] : score_map) {
            if (score > max_score) {
                max_score = score;
                tied_max.clear();
                tied_max.emplace_back(key, z_map.at(key));
            } else if (score == max_score && score > 0) {
                tied_max.emplace_back(key, z_map.at(key));
            }
        }

        if (max_score <= 0) {
            break;
        }
        std::sort(tied_max.begin(), tied_max.end(), [](auto const& lhs, auto const& rhs) {
            return integer_vec_less(lhs.first, rhs.first);
        });
        size_t chosen_idx = 0;
        if (tie_runtime != nullptr && tie_runtime->enabled) {
            size_t const step_index = tie_runtime->report.tohpe_step_count;
            chosen_idx = choose_controlled_tie_index(
                *tie_runtime, FastToddTieLevel::tohpe, step_index, tied_max.size());
            record_controlled_decision(
                *tie_runtime, FastToddTieLevel::tohpe, step_index, tied_max.size(), chosen_idx);
            tie_runtime->report.tohpe_step_count++;
        }
        auto const max_z = tied_max[chosen_idx].second;

        auto const terms_before_move = table.num_rows();
        std::vector<unsigned char> to_update{y.begin(), y.begin() + static_cast<long>(n_terms)};
        if (y_parity) {
            table.push_zeros_row();
            extended_matrix.push_row(build_extended_row_todd(dvlab::BooleanMatrix::Row(n_qubits, 0), n_qubits));
            augmented_matrix.push_zeros_column();
            augmented_matrix.push_zeros_row();
            augmented_matrix[augmented_matrix.num_rows() - 1][augmented_matrix.num_cols() - 1] = 1;
            to_update.push_back(1);
        }
        for (auto i : std::views::iota(0ul, to_update.size())) {
            if (to_update[i] == 0) continue;
            table[i] += max_z;
        }

        std::unordered_map<IntegerVec, size_t, IntegerVecHash> hashmap{};
        std::vector<size_t> to_remove{};
        n_terms = table.num_rows();
        for (auto i : std::views::iota(0ul, n_terms)) {
            if (table[i].is_zeros()) {
                to_remove.push_back(i);
                continue;
            }
            auto const col = row_to_integer_vec(table[i]);
            auto const hit = hashmap.find(col);
            if (hit != hashmap.end()) {
                to_remove.push_back(hit->second);
                to_remove.push_back(i);
                hashmap.erase(hit);
            } else {
                hashmap.emplace(col, i);
            }
        }
        std::sort(to_remove.begin(), to_remove.end(), std::greater<size_t>());

        for (auto const& i : to_remove) {
            clear_column(i, extended_matrix, augmented_matrix, pivots);
            table[i] = table[table.num_rows() - 1];
            table.erase_row(table.num_rows() - 1);
            extended_matrix[i] = extended_matrix[extended_matrix.num_rows() - 1];
            extended_matrix.erase_row(extended_matrix.num_rows() - 1);
            augmented_matrix[i] = augmented_matrix[augmented_matrix.num_rows() - 1];
            augmented_matrix.erase_row(augmented_matrix.num_rows() - 1);
            to_update[i] = to_update.back();
            to_update.pop_back();
            if (pivots.contains(table.num_rows())) {
                pivots[i] = pivots[table.num_rows()];
                pivots.erase(table.num_rows());
            }
            auto const row_count = table.num_rows();
            for (auto j : std::views::iota(0ul, augmented_matrix.num_rows())) {
                auto& row = augmented_matrix[j];
                if (row.size() <= row_count) continue;

                if (i < row.size()) {
                    if (row[i] != row[row_count]) {
                        row[i] ^= 1;
                    }
                }
                if (row[row_count]) {
                    row[row_count] ^= 1;
                }
            }
        }

        auto const aug_width = table.num_rows();
        for (auto i : std::views::iota(0ul, aug_width)) {
            if (augmented_matrix[i].size() > aug_width) {
                std::vector<unsigned char> trimmed(augmented_matrix[i].begin(), augmented_matrix[i].begin() + static_cast<ptrdiff_t>(aug_width));
                augmented_matrix[i].set_row(std::move(trimmed));
            }
        }

        std::vector<size_t> update_indices{};
        update_indices.reserve(to_update.size());
        for (auto i : std::views::iota(0ul, to_update.size())) {
            if (to_update[i]) {
                update_indices.push_back(i);
            }
        }
        for (auto const i : update_indices) {
            clear_column(i, extended_matrix, augmented_matrix, pivots);
            extended_matrix[i] = build_extended_row_todd(table[i], n_qubits);
            std::vector<unsigned char> bv(table.num_rows(), 0);
            bv[i] = 1;
            augmented_matrix[i].set_row(bv);
        }

        ++move_idx;
        if (move_idx >= max_moves) {
            return;
        }
    }
}

Polynomial tohpe_once_iteration(Polynomial const& polynomial, size_t max_moves = static_cast<size_t>(-1)) {
    if (polynomial.empty()) {
        return polynomial;
    }
    auto const n_qubits = polynomial.front().n_qubits();
    auto       table    = dvlab::transpose(load_phase_poly_matrix(polynomial));
    tohpe_table_pass(table, n_qubits, max_moves);
    return from_boolean_matrix(table);
}

std::unordered_map<size_t, size_t> invert_pivot_map(std::unordered_map<size_t, size_t> const& row_to_col) {
    std::unordered_map<size_t, size_t> col_to_row;
    col_to_row.reserve(row_to_col.size());
    for (auto const& [row, col] : row_to_col) {
        col_to_row[col] = row;
    }
    return col_to_row;
}

void fold_pivot_columns(
    dvlab::BooleanMatrix::Row&                col,
    dvlab::BooleanMatrix::Row&                aug_col,
    size_t                                    pivot_col,
    std::unordered_map<size_t, size_t> const& col_to_row,
    dvlab::BooleanMatrix const&               matrix,
    dvlab::BooleanMatrix const&               augmented_matrix) {
    auto const it = col_to_row.find(pivot_col);
    if (it == col_to_row.end()) {
        return;
    }
    size_t const row = it->second;
    col += matrix[row];
    aug_col += augmented_matrix[row];
}

void eliminate_r_matrices(
    std::vector<dvlab::BooleanMatrix::Row>& r_mat,
    std::vector<dvlab::BooleanMatrix::Row>& augmented_r_mat) {
    for (size_t k = 0; k < r_mat.size(); ++k) {
        size_t const index =
            std::distance(r_mat[k].begin(), std::find(r_mat[k].begin(), r_mat[k].end(), true));
        if (r_mat[k][index]) {
            auto const pivot           = r_mat[k];
            auto const augmented_pivot = augmented_r_mat[k];
            for (size_t l = k + 1; l < r_mat.size(); ++l) {
                if (r_mat[l][index]) {
                    r_mat[l] += pivot;
                    augmented_r_mat[l] += augmented_pivot;
                }
            }
        }
    }
}

int score_fast_todd_move(
    dvlab::BooleanMatrix& table,
    dvlab::BooleanMatrix::Row const& z,
    dvlab::BooleanMatrix::Row const& y,
    std::unordered_map<IntegerVec, size_t, IntegerVecHash> const& term_index) {
    int score = 0;
    for (size_t l = 0; l < table.num_rows(); ++l) {
        if (!y[l]) {
            continue;
        }
        table[l] += z;
        auto const it = term_index.find(row_to_integer_vec(table[l]));
        if (it != term_index.end() && !y[it->second]) {
            score += 2;
        }
        table[l] += z;
    }
    if (y.sum() % 2 == 1) {
        if (term_index.contains(row_to_integer_vec(z))) {
            score += 1;
        } else {
            score -= 1;
        }
    }
    return score;
}

void proper_term_table(dvlab::BooleanMatrix& table) {
    std::unordered_map<IntegerVec, size_t, IntegerVecHash> seen;
    std::vector<size_t>                                    to_remove;
    for (size_t i = 0; i < table.num_rows(); ++i) {
        size_t const first_one =
            std::distance(table[i].begin(), std::find(table[i].begin(), table[i].end(), true));
        if (first_one >= table[i].size() || !table[i][first_one]) {
            to_remove.push_back(i);
            continue;
        }
        auto const col = row_to_integer_vec(table[i]);
        auto const it  = seen.find(col);
        if (it != seen.end()) {
            to_remove.push_back(it->second);
            to_remove.push_back(i);
            seen.erase(it);
        } else {
            seen.emplace(col, i);
        }
    }
    std::sort(to_remove.begin(), to_remove.end(), std::greater<size_t>());
    for (size_t const i : to_remove) {
        table[i] = table[table.num_rows() - 1];
        table.erase_row(table.num_rows() - 1);
    }
}

/**
 * One FastTODD step on term×qubit table.
 * @return false when max_score == 0 (no move); true after applying best move.
 */
bool fast_todd_table_step(dvlab::BooleanMatrix& table, size_t n_qubits, size_t outer_iter = 0,
                          char const* backend = "cpp",
                          FastToddTieRuntime* tie_runtime = nullptr) {
    if (table.num_rows() == 0) {
        return false;
    }

    dvlab::BooleanMatrix extended_matrix;
    extended_matrix.reserve(table.num_rows(), table.num_cols());
    for (size_t i = 0; i < table.num_rows(); ++i) {
        extended_matrix.push_row(build_extended_row_todd(table[i], n_qubits));
    }

    dvlab::BooleanMatrix augmented_matrix = dvlab::identity(table.num_rows());
    std::unordered_map<size_t, size_t> pivots;
    (void)kernel(extended_matrix, augmented_matrix, pivots);
    auto const col_to_row = invert_pivot_map(pivots);

    // Extended columns use block-padded width (e.g. 2560 for 2556 logical ext bits).
    size_t const logical_ext = n_qubits + (n_qubits * (n_qubits - 1)) / 2;
    size_t const ext_width   = block_bit_width(logical_ext);
    for (size_t r = 0; r < extended_matrix.num_rows(); ++r) {
        pad_row_to_width(extended_matrix[r], ext_width);
    }

    std::unordered_map<IntegerVec, size_t, IntegerVecHash> term_index;
    for (size_t i = 0; i < table.num_rows(); ++i) {
        term_index[row_to_integer_vec(table[i])] = i;
    }

    struct ScoredMove {
        dvlab::BooleanMatrix::Row z;
        dvlab::BooleanMatrix::Row y;
        size_t                    i = 0;
        size_t                    j = 0;
    };

    int                     max_score = 0;
    std::vector<ScoredMove> best_moves;

    for (size_t i = 0; i < table.num_rows(); ++i) {
        for (size_t j = i + 1; j < table.num_rows(); ++j) {
            if (stop_requested()) {
                return false;
            }

            auto z = table[i];
            z += table[j];

            std::vector<dvlab::BooleanMatrix::Row> r_mat;
            std::vector<dvlab::BooleanMatrix::Row> augmented_r_mat;
            r_mat.reserve(n_qubits + 1);
            augmented_r_mat.reserve(n_qubits + 1);

            for (size_t k = 0; k < n_qubits; ++k) {
                dvlab::BooleanMatrix::Row col(ext_width, 0);
                dvlab::BooleanMatrix::Row aug_col(table.num_rows(), 0);
                size_t l = 0;
                for (size_t a = n_qubits; a-- > 0;) {
                    for (size_t b = 0; b < a; ++b) {
                        if ((a == k && z[b]) || (b == k && z[a])) {
                            size_t const pivot_col = n_qubits + l;
                            col[pivot_col] ^= 1;
                            fold_pivot_columns(col, aug_col, pivot_col, col_to_row, extended_matrix, augmented_matrix);
                        }
                        ++l;
                    }
                }
                r_mat.push_back(col);
                augmented_r_mat.push_back(aug_col);
            }

            {
                dvlab::BooleanMatrix::Row col(ext_width, 0);
                dvlab::BooleanMatrix::Row aug_col(table.num_rows(), 0);
                size_t l = 0;
                for (size_t a = n_qubits; a-- > 0;) {
                    for (size_t b = 0; b < a; ++b) {
                        if (z[a] && z[b]) {
                            size_t const pivot_col = n_qubits + l;
                            col[pivot_col] ^= 1;
                            fold_pivot_columns(col, aug_col, pivot_col, col_to_row, extended_matrix, augmented_matrix);
                        }
                        ++l;
                    }
                    if (z[a]) {
                        col[a] ^= 1;
                        fold_pivot_columns(col, aug_col, a, col_to_row, extended_matrix, augmented_matrix);
                    }
                }
                r_mat.push_back(col);
                augmented_r_mat.push_back(aug_col);
            }

            for (size_t k = 0; k < r_mat.size(); ++k) {
                size_t index = 0;
                for (size_t t = 0; t < r_mat[k].size(); ++t) {
                    if (r_mat[k][t]) {
                        index = t;
                        break;
                    }
                }
                if (r_mat[k][index]) {
                    auto const pivot           = r_mat[k];
                    auto const augmented_pivot = augmented_r_mat[k];
                    for (size_t l = k + 1; l < r_mat.size(); ++l) {
                        if (r_mat[l][index]) {
                            r_mat[l] += pivot;
                            augmented_r_mat[l] += augmented_pivot;
                        }
                    }
                } else if (i < augmented_r_mat[k].size() && j < augmented_r_mat[k].size() &&
                           augmented_r_mat[k][i] != augmented_r_mat[k][j]) {
                    auto const y     = augmented_r_mat[k];
                    int const  score = score_fast_todd_move(table, z, y, term_index);
                    if (score > max_score) {
                        max_score = score;
                        best_moves.clear();
                        best_moves.push_back(
                            ScoredMove{dvlab::BooleanMatrix::Row(z.get_row()),
                                       dvlab::BooleanMatrix::Row(y.get_row()), i, j});
                    } else if (score == max_score && score > 0) {
                        best_moves.push_back(
                            ScoredMove{dvlab::BooleanMatrix::Row(z.get_row()),
                                       dvlab::BooleanMatrix::Row(y.get_row()), i, j});
                    }
                }
            }
        }
    }

    if (max_score <= 0 || best_moves.empty()) {
        if (fasttodd_trace_enabled()) {
            fmt::print(stderr, "[{}-fasttodd] outer={} stop score={} rows={}\n",
                       backend, outer_iter, max_score, table.num_rows());
        }
        return false;
    }

    size_t chosen_idx = 0;
    bool controlled_tie_break = false;
    if (tie_runtime != nullptr && tie_runtime->enabled) {
        size_t const step_index = tie_runtime->report.outer_step_count;
        chosen_idx = choose_controlled_tie_index(
            *tie_runtime, FastToddTieLevel::outer, step_index, best_moves.size());
        record_controlled_decision(
            *tie_runtime, FastToddTieLevel::outer, step_index, best_moves.size(), chosen_idx);
        tie_runtime->report.outer_step_count++;
        controlled_tie_break = true;
    } else if (best_moves.size() > 1 && fasttodd_random_tie_break_enabled()) {
        // ponytail: random tie break is for exploring FastTODD branches; deterministic mode remains default.
        std::uniform_int_distribution<size_t> dist(0, best_moves.size() - 1);
        chosen_idx = dist(fasttodd_rng());
    }
    auto const& chosen = best_moves[chosen_idx];

    if (best_moves.size() > 1 && (fasttodd_trace_enabled() || fasttodd_trace_ties_enabled())) {
        fmt::print(stderr,
                   "[{}-fasttodd] outer={} max_score={} tied_moves={} tie_break={}\n",
                   backend, outer_iter, max_score, best_moves.size(),
                   controlled_tie_break ? "scripted" :
                   (fasttodd_random_tie_break_enabled() ? "random" : "first"));
    }

    for (size_t l = 0; l < table.num_rows(); ++l) {
        if (chosen.y[l]) {
            table[l] += chosen.z;
        }
    }
    if (chosen.y.sum() % 2 == 1) {
        table.push_row(chosen.z);
    }
    proper_term_table(table);
    trace_chosen_move(backend, outer_iter, chosen.i, chosen.j, max_score, chosen.z, chosen.y, table.num_rows());
    return true;
}

/**
 * Each iteration runs full tohpe() then one FastTODD table step;
 * loop until max_score == 0 (fast_todd_table_step returns false).
 */
Polynomial fasttodd_once(Polynomial const& polynomial, char const* backend) {
    if (polynomial.empty()) {
        return polynomial;
    }

    auto         table      = dvlab::transpose(load_phase_poly_matrix(polynomial));
    size_t const n_qubits   = table.num_cols();
    trace_dump_table("input", backend, table);

    FastToddTieRuntime tie_runtime;
    FastToddTieRuntime* tie_runtime_ptr = nullptr;
    if (g_fasttodd_tie_control.has_value() && g_fasttodd_tie_control->enabled) {
        tie_runtime.enabled                     = true;
        tie_runtime.mode                        = g_fasttodd_tie_control->mode;
        tie_runtime.target_random_step          = g_fasttodd_tie_control->target_random_step;
        tie_runtime.forced_tohpe_choice_by_step = g_fasttodd_tie_control->forced_tohpe_choice_by_step;
        tie_runtime.forced_outer_choice_by_step = g_fasttodd_tie_control->forced_outer_choice_by_step;
        if (g_fasttodd_tie_control->random_seed.has_value()) {
            tie_runtime.rng.seed(*g_fasttodd_tie_control->random_seed);
        } else {
            tie_runtime.rng.seed(std::random_device{}());
        }
        tie_runtime.report.initial_term_count = table.num_rows();
        tie_runtime_ptr = &tie_runtime;
    }

    size_t outer = 0;
    while (true) {
        tohpe_table_pass(table, n_qubits, static_cast<size_t>(-1), tie_runtime_ptr);
        if (table.num_rows() == 0) {
            break;
        }
        if (fasttodd_trace_enabled()) {
            fmt::print(stderr, "[{}-fasttodd] outer={} after_tohpe rows={}\n", backend, outer, table.num_rows());
        }
        if (!fast_todd_table_step(table, n_qubits, outer, backend, tie_runtime_ptr)) {
            break;
        }
        ++outer;
    }
    if (fasttodd_trace_enabled()) {
        fmt::print(stderr, "[{}-fasttodd] done outer_iters={} final_rows={}\n", backend, outer, table.num_rows());
        trace_dump_table("output", backend, table);
    }
    if (tie_runtime_ptr != nullptr) {
        tie_runtime.report.final_term_count = table.num_rows();
    }
    if (tie_runtime_ptr == nullptr) {
        g_fasttodd_tie_run_report = std::nullopt;
    } else {
        g_fasttodd_tie_run_report = tie_runtime.report;
    }
    return from_boolean_matrix(table);
}

Polynomial fasttodd_once(Polynomial const& polynomial) {
    return fasttodd_once(polynomial, "cpp");
}

/** Original C++ FastTODD optimize path (properize → fasttodd_once → Clifford correction). */
std::optional<std::pair<StabilizerTableau, Polynomial>> fasttodd_optimize(
    StabilizerTableau const& clifford,
    Polynomial const&        polynomial) {
    auto ret_clifford   = clifford;
    auto ret_polynomial = polynomial;

    properize(ret_clifford, ret_polynomial);

    auto multi_linear_polynomial = MultiLinearPolynomial();
    multi_linear_polynomial.add_rotations(ret_polynomial, false);

    ret_polynomial = fasttodd_once(ret_polynomial);

    multi_linear_polynomial.add_rotations(ret_polynomial, true);

    if (auto clifford_ops = multi_linear_polynomial.extract_clifford_operators(); clifford_ops.has_value()) {
        ret_clifford.apply(*clifford_ops);
        return std::make_pair(std::move(ret_clifford), std::move(ret_polynomial));
    }
    return std::nullopt;
}

}  // namespace

void set_fasttodd_tie_control(std::optional<FastToddTieControl> control) {
    g_fasttodd_tie_control = std::move(control);
}

std::optional<FastToddTieRunReport> consume_fasttodd_tie_run_report() {
    auto out = g_fasttodd_tie_run_report;
    g_fasttodd_tie_run_report = std::nullopt;
    return out;
}

std::pair<StabilizerTableau, Polynomial> TohpePhasePolynomialOptimizationStrategy::optimize(StabilizerTableau const& clifford, Polynomial const& polynomial) const {
    if (polynomial.empty()) {
        fmt::println("Polynomial is empty, returning the input Clifford and polynomial");
        return {clifford, polynomial};
    }

    if (std::ranges::any_of(polynomial, [](PauliRotation const& rotation) { return 4 % rotation.phase().denominator() != 0; })) {
        spdlog::error("Failed to perform TODD optimization: the polynomial contains a non-4th-root-of-unity phase!!");
        return {clifford, polynomial};
    }

    auto ret_clifford   = clifford;
    auto ret_polynomial = polynomial;

    properize(ret_clifford, ret_polynomial);

    auto multi_linear_polynomial = MultiLinearPolynomial();
    multi_linear_polynomial.add_rotations(ret_polynomial, false);

    while (true) {
        auto const num_terms = ret_polynomial.size();
        ret_polynomial       = tohpe_once_iteration(ret_polynomial);
        if (ret_polynomial.empty() || ret_polynomial.size() == num_terms) {
            break;
        }
    }

    auto optimized_multi_linear_polynomial = MultiLinearPolynomial();
    optimized_multi_linear_polynomial.add_rotations(ret_polynomial, false);
    if (!multi_linear_polynomial.has_same_signature_tensor(optimized_multi_linear_polynomial)) {
        spdlog::error("Failed to perform TOHPE optimization: the post-optimization polynomial does not have the same signature tensor as the pre-optimization polynomial!!");
        return {clifford, polynomial};
    }

    multi_linear_polynomial.add_rotations(ret_polynomial, true);

    if (auto clifford_ops = multi_linear_polynomial.extract_clifford_operators(); clifford_ops.has_value()) {
        ret_clifford.apply(*clifford_ops);
    } else {
        spdlog::error("Failed to perform TOHPE optimization: the post-optimization polynomial does not have the same signature as the pre-optimization polynomial!!");
        return {clifford, polynomial};
    }

    return {ret_clifford, ret_polynomial};
}

std::pair<StabilizerTableau, Polynomial> FastToddPhasePolynomialOptimizationStrategy::optimize(StabilizerTableau const& clifford, Polynomial const& polynomial) const {
    if (polynomial.empty()) {
        fmt::println("Polynomial is empty, returning the input Clifford and polynomial");
        return {clifford, polynomial};
    }

    if (std::ranges::any_of(polynomial, [](PauliRotation const& rotation) { return 4 % rotation.phase().denominator() != 0; })) {
        spdlog::error("Failed to perform FastTODD optimization: the polynomial contains a non-4th-root-of-unity phase!!");
        return {clifford, polynomial};
    }

    auto const initial_t = polynomial.size();
    auto const result    = fasttodd_optimize(clifford, polynomial);
    if (!result.has_value()) {
        spdlog::error("Failed to perform FastTODD optimization: the post-optimization polynomial does not have the same signature as the pre-optimization polynomial!!");
        return {clifford, polynomial};
    }

    auto const final_t = result->second.size();
    spdlog::info("FastTODD: T count {} -> {}", initial_t, final_t);
    return *result;
}

}  // namespace qsyn::experimental
