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

    // Read header
    while (std::getline(file, line)) {
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
                    spdlog::error("Invalid number of qubits in file");
                    return false;
                }
            } else if (line.find("# Number of gates:") != std::string::npos) {
                try {
                    num_gates = std::stoul(line.substr(line.find(':') + 1));
                } catch (std::exception const&) {
                    spdlog::error("Invalid number of gates in file");
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

    // Reset the circuit and add qubits
    ls.reset();
    ls.add_qubits(num_qubits);

    // Read operations
    while (std::getline(file, line)) {
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
                    spdlog::error("Qubit ID {} out of range (max: {})", qubit_id, num_qubits - 1);
                    return false;
                }
                qubits.push_back(qubit_id);
            }

            if (qubits.empty()) {
                spdlog::error("No qubits specified for operation");
                return false;
            }

            LatticeSurgeryOpType op_type = (op == "merge") ? LatticeSurgeryOpType::merge : LatticeSurgeryOpType::split;
            LatticeSurgeryGate gate(op_type, qubits);
            ls.append(gate);
        } else {
            spdlog::error("Unknown operation type: {}", op);
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