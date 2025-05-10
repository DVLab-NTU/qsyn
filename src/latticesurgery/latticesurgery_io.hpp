/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Define I/O operations for LatticeSurgery ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#pragma once

#include <filesystem>
#include <string>

#include "./latticesurgery.hpp"

namespace qsyn::latticesurgery {

// File format constants
constexpr char const* LS_FILE_HEADER = "# Lattice Surgery Circuit";
constexpr char const* LS_QUBIT_DECL = "qubit";
constexpr char const* LS_MERGE_OP = "merge";
constexpr char const* LS_SPLIT_OP = "split";

// File format example:
// # Lattice Surgery Circuit
// qubit q0
// qubit q1
// qubit q2
// merge q0 q1 q2
// split q0 q1
// merge q1 q2

bool write_ls_file(std::filesystem::path const& filepath, LatticeSurgery const& ls);
bool read_ls_file(std::filesystem::path const& filepath, LatticeSurgery& ls);

std::optional<LatticeSurgery> from_file(std::filesystem::path const& filepath);

std::string to_ls(LatticeSurgery const& ls);

} // namespace qsyn::latticesurgery 