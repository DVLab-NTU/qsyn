/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Implementation of LatticeSurgery reader ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include "latticesurgery/latticesurgery.hpp"
#include "latticesurgery/latticesurgery_io.hpp"

#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <fstream>
#include <optional>
#include <string>

namespace qsyn::latticesurgery {

bool read_ls_file(std::filesystem::path const& filepath, LatticeSurgery& ls) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        spdlog::error("Cannot open file {}", filepath.string());
        return false;
    }

    std::string line;
    bool header_found = false;
    size_t num_qubits = 0;
    size_t num_gates = 0;
    size_t grid_rows = 0;
    size_t grid_cols = 0;
    size_t line_number = 0;

    // Read header
    while (std::getline(file, line)) {
        line_number++;
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#') {
            if (line.find("# Lattice Surgery Circuit") != std::string::npos) {
                header_found = true;
            } else if (line.find("# Number of qubits:") != std::string::npos) {
                try {
                    num_qubits = std::stoul(line.substr(line.find(':') + 1));
                } catch (std::exception const&) {
                    spdlog::error("Invalid number of qubits in file at line {}", line_number-1);
                    return false;
                }
            } else if (line.find("# Number of gates:") != std::string::npos) {
                try {
                    num_gates = std::stoul(line.substr(line.find(':') + 1));
                } catch (std::exception const&) {
                    spdlog::error("Invalid number of gates in file at line {}", line_number-1);
                    return false;
                }
            } else if (line.find("# Grid dimensions:") != std::string::npos) {
                try {
                    std::string dims = line.substr(line.find(':') + 1);
                    size_t x_pos = dims.find('x');
                    if (x_pos == std::string::npos) {
                        spdlog::error("Invalid grid dimensions format at line {}", line_number-1);
                        return false;
                    }
                    grid_rows = std::stoul(dims.substr(0, x_pos));
                    grid_cols = std::stoul(dims.substr(x_pos + 1));
                } catch (std::exception const&) {
                    spdlog::error("Invalid grid dimensions in file at line {}", line_number-1);
                    return false;
                }
            }
            continue;
        }
        // If we find a non-header line, put it back in the stream
        file.seekg(-static_cast<std::streamoff>(line.length() + 1), std::ios::cur);
        break;
    }

    if (!header_found) {
        spdlog::error("Missing header in file");
        return false;
    }

    if (grid_rows == 0 || grid_cols == 0) {
        spdlog::error("Grid dimensions not specified in file");
        return false;
    }

    // Reset the circuit and add grid
    ls.reset();
    ls.get_grid() = LatticeSurgeryGrid(grid_rows, grid_cols);
    ls.add_qubits(grid_rows * grid_cols);
    ls.init_logical_tracking(grid_rows * grid_cols);

    // Read operations
    while (std::getline(file, line)) {
        line_number++;
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string op;
        iss >> op;

        if (op == "merge" || op == "split") {
            std::vector<QubitIdType> qubits;
            QubitIdType qubit_id;
            while (iss >> qubit_id) {
                if (qubit_id >= num_qubits) {
                    spdlog::error("Qubit ID {} out of range (max: {}) at line {}", qubit_id, num_qubits - 1, line_number);
                    return false;
                }
                qubits.push_back(qubit_id);
            }

            if (qubits.empty()) {
                spdlog::error("No qubits specified for operation at line {}", line_number-1);
                return false;
            }

            // Validate and perform the operation
            if (op == "merge") {
                if (!ls.merge_patches(qubits)) {
                    spdlog::error("Failed to merge patches at line {}", line_number-1);
                    return false;
                }
            } else {  // split
                if (!ls.split_patches(qubits)) {
                    spdlog::error("Failed to split patches at line {}", line_number-1);
                    return false;
                }
            }

            // Add the gate to the circuit
            LatticeSurgeryOpType op_type = (op == "merge") ? LatticeSurgeryOpType::merge : LatticeSurgeryOpType::split;
            LatticeSurgeryGate gate(op_type, qubits);
            ls.append(gate);
        } else {
            spdlog::error("Unknown operation type: {} at line {}", op, line_number);
            return false;
        }
    }

    return true;
}

std::optional<LatticeSurgery> from_file(std::filesystem::path const& filepath) {
    auto const extension = filepath.extension();

    if (extension == ".ls") {
        LatticeSurgery ls;
        if (read_ls_file(filepath, ls)) {
            return ls;
        }
    } else {
        spdlog::error("File format \"{}\" is not supported!!", extension);
    }
    return std::nullopt;
}

} // namespace qsyn::latticesurgery 