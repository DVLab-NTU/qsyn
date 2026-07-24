/****************************************************************************
  PackageName  [ qcir / quick_partitioner ]
  Synopsis     [ QuickPartitioner and adjacent-region merge. ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "./quick_partitioner.hpp"

#include <algorithm>
#include <map>

namespace qsyn::qcir {

namespace {

[[nodiscard]] std::set<QubitIdType> gate_qubit_set(QCirGate const& gate) {
    auto qs = gate.get_qubits();
    return {qs.begin(), qs.end()};
}

[[nodiscard]] std::set<QubitIdType> union_qubits(std::set<QubitIdType> const& a,
                                                 std::set<QubitIdType> const& b) {
    auto out = a;
    out.insert(b.begin(), b.end());
    return out;
}

}  // namespace

std::vector<UnitaryRegion> quick_partition(QCir const& circuit, size_t block_size) {
    if (block_size < 2) block_size = 2;

    std::vector<UnitaryRegion> regions;
    UnitaryRegion cur;

    for (auto const* gate : circuit.get_gates()) {
        auto const gate_qs = gate_qubit_set(*gate);
        auto const trial   = union_qubits(cur.qubits, gate_qs);

        if (!cur.gates.empty() && trial.size() > block_size) {
            regions.push_back(std::move(cur));
            cur = {};
        }

        cur.gates.push_back(gate);
        cur.qubits = union_qubits(cur.qubits, gate_qs);
    }

    if (!cur.gates.empty()) {
        regions.push_back(std::move(cur));
    }
    return regions;
}

std::vector<UnitaryRegion> quick_partition_with_merge(QCir const& circuit, size_t block_size) {
    auto regions = quick_partition(circuit, block_size);
    if (regions.size() < 2) return regions;

    bool merged = true;
    while (merged) {
        merged = false;
        std::vector<UnitaryRegion> next;
        next.reserve(regions.size());

        for (size_t i = 0; i < regions.size(); ++i) {
            if (i + 1 < regions.size()) {
                auto const trial_qs =
                    union_qubits(regions[i].qubits, regions[i + 1].qubits);
                if (trial_qs.size() <= block_size) {
                    UnitaryRegion combined;
                    combined.gates = regions[i].gates;
                    combined.gates.insert(combined.gates.end(), regions[i + 1].gates.begin(),
                                          regions[i + 1].gates.end());
                    combined.qubits = trial_qs;
                    next.push_back(std::move(combined));
                    ++i;
                    merged = true;
                    continue;
                }
            }
            next.push_back(std::move(regions[i]));
        }
        regions = std::move(next);
    }
    return regions;
}

std::vector<UnitaryRegion> split_entangling_boundaries(UnitaryRegion const& region) {
    if (region.gates.size() <= 1) return {region};

    auto has_entangling = [](UnitaryRegion const& r) {
        return std::ranges::any_of(r.gates,
                                   [](QCirGate const* g) { return g->get_num_qubits() >= 2; });
    };

    std::vector<UnitaryRegion> out;
    UnitaryRegion cur;

    auto flush = [&]() {
        if (!cur.gates.empty()) out.push_back(std::move(cur));
        cur = {};
    };

    for (auto const* gate : region.gates) {
        auto qs = gate_qubit_set(*gate);

        if (!cur.gates.empty() && gate->get_num_qubits() >= 2) {
            flush();
        }
        if (!cur.gates.empty() && gate->get_num_qubits() == 1 && has_entangling(cur)) {
            flush();
        }

        cur.gates.push_back(gate);
        cur.qubits = union_qubits(cur.qubits, qs);
    }
    flush();
    return out;
}

std::vector<UnitaryRegion> scan_partition(QCir const& circuit, size_t block_size) {
    if (block_size < 2) block_size = 2;

    auto const& gates = circuit.get_gates();
    auto const n      = gates.size();
    if (n == 0) return {};

    std::vector<bool> used(n, false);
    std::vector<UnitaryRegion> regions;

    auto score_range = [&](size_t start, size_t end) -> int {
        int s = 0;
        for (size_t i = start; i <= end; ++i) {
            if (gates[i]->get_num_qubits() >= 2) ++s;
        }
        return s;
    };

    auto build_region = [&](size_t start, size_t end) -> UnitaryRegion {
        UnitaryRegion r;
        for (size_t i = start; i <= end; ++i) {
            r.gates.push_back(gates[i]);
            r.qubits = union_qubits(r.qubits, gate_qubit_set(*gates[i]));
        }
        return r;
    };

    for (;;) {
        int best_score = -1;
        size_t best_s = 0, best_e = 0;
        bool found = false;

        for (size_t start = 0; start < n; ++start) {
            if (used[start]) continue;
            std::set<QubitIdType> qs;
            for (size_t end = start; end < n; ++end) {
                if (used[end]) break;
                qs = union_qubits(qs, gate_qubit_set(*gates[end]));
                if (qs.size() > block_size) break;
                auto const sc = score_range(start, end);
                if (sc > best_score) {
                    best_score = sc;
                    best_s     = start;
                    best_e     = end;
                    found      = true;
                }
            }
        }

        if (!found) break;

        for (size_t i = best_s; i <= best_e; ++i) used[i] = true;
        regions.push_back(build_region(best_s, best_e));
    }

    // Preserve circuit order: sort regions by first gate index.
    std::ranges::sort(regions, [&](UnitaryRegion const& a, UnitaryRegion const& b) {
        return a.gates.front()->get_id() < b.gates.front()->get_id();
    });

    return regions;
}

std::vector<UnitaryRegion> partition_circuit(QCir const& circuit, size_t block_size,
                                             PartitionStrategy strategy) {
    switch (strategy) {
        case PartitionStrategy::Scan:
            return scan_partition(circuit, block_size);
        case PartitionStrategy::Quick:
            return quick_partition(circuit, block_size);
        case PartitionStrategy::QuickScan:
        default: {
            auto regions = quick_partition_with_merge(circuit, block_size);
            if (regions.size() <= 1) return regions;
            // Refine oversized merged regions with scan on the sub-circuit slice.
            std::vector<UnitaryRegion> refined;
            for (auto const& region : regions) {
                if (region.gates.size() <= 2 || region.qubits.size() < block_size) {
                    refined.push_back(region);
                    continue;
                }
                QCir slice{region.qubits.size()};
                std::map<QubitIdType, QubitIdType> to_local;
                size_t next = 0;
                for (auto q : region.qubits) to_local[q] = static_cast<QubitIdType>(next++);

                std::vector<QCirGate const*> slice_to_orig;
                slice_to_orig.reserve(region.gates.size());
                for (auto const* g : region.gates) {
                    QubitIdList lq;
                    for (auto qb : g->get_qubits()) lq.push_back(to_local.at(qb));
                    slice.append(g->get_operation(), lq);
                    slice_to_orig.push_back(g);
                }

                auto const& slice_gates = slice.get_gates();
                auto sub                = scan_partition(slice, block_size);
                for (auto const& part : sub) {
                    UnitaryRegion global;
                    for (auto const* lg : part.gates) {
                        for (size_t j = 0; j < slice_gates.size(); ++j) {
                            if (slice_gates[j] == lg) {
                                global.gates.push_back(slice_to_orig[j]);
                                break;
                            }
                        }
                    }
                    for (auto qb : part.qubits) {
                        for (auto const& [g, l] : to_local) {
                            if (l == qb) global.qubits.insert(g);
                        }
                    }
                    if (!global.gates.empty()) refined.push_back(std::move(global));
                }
            }
            if (!refined.empty()) return refined;
            return regions;
        }
    }
}

}  // namespace qsyn::qcir
