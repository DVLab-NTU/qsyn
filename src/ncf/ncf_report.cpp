#include "ncf/ncf_types.hpp"

#include <fmt/core.h>

namespace qsyn::experimental {

void NcfGateCost::print(std::string const& label) const {
    fmt::println("  {}:", label);
    fmt::println("    CX: {}  RZ: {}  H: {}  S: {}  Sdg: {}  clifford: {}  total: {}  depth: {}",
                 cx, rz, h, s, sdg, clifford_total, total_gates, depth);
}

void NcfFusionReport::print() const {
    fmt::println("[NCF fusion report]");
    fmt::println("  Pauli terms (before):     {}   (indices #0–#{})", n_pauli_terms, n_pauli_terms == 0 ? 0 : n_pauli_terms - 1);
    fmt::println("  NCF blocks (after):       {}", n_blocks);
    fmt::println("  Synthesis: before={}  after={}", synth_before, synth_after);
    before.print("Cost BEFORE NCF");
    after.print("Cost AFTER NCF");
    fmt::println("  Delta (after − before):");
    fmt::println("    CX: {}  RZ: {}  clifford: {}  total: {}",
                 static_cast<long long>(after.cx) - static_cast<long long>(before.cx),
                 static_cast<long long>(after.rz) - static_cast<long long>(before.rz),
                 static_cast<long long>(after.clifford_total) - static_cast<long long>(before.clifford_total),
                 static_cast<long long>(after.total_gates) - static_cast<long long>(before.total_gates));
}

}  // namespace qsyn::experimental
