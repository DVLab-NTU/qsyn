/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Define class LatticeSurgery Writer functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/ostream.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "./latticesurgery.hpp"
#include "./latticesurgery_gate.hpp"
#include "./latticesurgery_io.hpp"

namespace qsyn::latticesurgery {

bool LatticeSurgery::write_ls(std::filesystem::path const& filepath) const {
    std::ofstream ofs(filepath);
    if (!ofs) {
        spdlog::error("Cannot open file {}", filepath.string());
        return false;
    }
    ofs << to_ls(*this);
    return true;
}

std::string to_ls(LatticeSurgery const& ls) {
    std::string output = "# Lattice Surgery Circuit\n";
    output += fmt::format("# Number of qubits: {}\n", ls.get_num_qubits());
    output += fmt::format("# Number of gates: {}\n", ls.get_num_gates());
    output += fmt::format("# Grid dimensions: {}x{}\n\n", ls.get_grid_rows(), ls.get_grid_cols());

    for (auto const* gate : ls.get_gates()) {
        output += fmt::format("{} {}\n",
                            gate->get_operation_type() == LatticeSurgeryOpType::merge ? "merge" : "split",
                            fmt::join(gate->get_qubits(), " "));
    }
    return output;
}

} // namespace qsyn::latticesurgery 