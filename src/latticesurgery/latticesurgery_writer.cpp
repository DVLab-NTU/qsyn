/****************************************************************************
  PackageName  [ latticesurgery ]
  Synopsis     [ Define class LatticeSurgery Writer functions ]
  Author       [ Design Verification Lab ]
  Copyright    [ Copyright(c) 2024 DVLab, GIEE, NTU, Taiwan ]
****************************************************************************/

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <cstddef>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <optional>
#include <unordered_map>

#include "./latticesurgery.hpp"
#include "./latticesurgery_gate.hpp"
#include "./latticesurgery_io.hpp"

namespace qsyn::latticesurgery {

using LatticeSurgeryOpType = qsyn::latticesurgery::LatticeSurgeryOpType;

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

bool LatticeSurgery::write_lasre(std::filesystem::path const& filepath) const {
    std::ofstream ofs(filepath);
    if (!ofs) {
        spdlog::error("Cannot open file {}", filepath.string());
        return false;
    }
    ofs << to_lasre();
    return true;
}

using json = nlohmann::json;

//-----------------------------------------------------------------------------
// helpers to build zero-filled arrays
static json init_3d(size_t ni, size_t nj, size_t nk, bool def = false) {
  json a = json::array();
  for (size_t i = 0; i < ni; ++i) {
    json row = json::array();
    for (size_t j = 0; j < nj; ++j) {
      json col = json::array();
      for (size_t k = 0; k < nk; ++k)
        col.push_back(def);
      row.push_back(std::move(col));
    }
    a.push_back(std::move(row));
  }
  return a;
}

static json init_4d(size_t ni, size_t nj, size_t nk, size_t ns, bool def = false) {
  json a = json::array();
  for (size_t s = 0; s < ns; ++s)
    a.push_back(init_3d(ni, nj, nk, def));
  return a;
}

//-----------------------------------------------------------------------------

std::string LatticeSurgery::to_lasre() const {
  json data;

  // dimensions
  size_t n_i = get_grid_cols();
  size_t n_j = get_grid_rows();
  size_t gate_depth = 0;
  
  auto gate_times = calculate_gate_times();
  size_t n_k = gate_times.empty() 
               ? 1 
               : (std::ranges::max(gate_times | std::views::values) + 1);
  // fmt::println("gate list: {}", _gate_list.size());
  for(auto gate: _gate_list){
    if(gate_depth < gate->get_depth()) gate_depth = gate->get_depth();
    fmt::println("gate {}: {}", gate->get_id(), gate->get_depth());
  }
  // fmt::println("gate depth: {}, gate time: {}", gate_depth, n_k);
  
  n_k = gate_depth+1;
  
  data["n_i"] = n_i;
  data["n_j"] = n_j;
  data["n_k"] = n_k;

  // gather all logical qubit IDs
  std::vector<QubitIdType> logical_ids;
  {
    std::vector<QubitIdType> seen;
    seen.reserve(n_i*n_j);
    for (size_t i = 0; i < n_i; ++i)
    for (size_t j = 0; j < n_j; ++j)
      if (auto* p = get_patch(i,j); p && p->occupied()) {
        auto id = p->get_logical_id();
        if (std::find(seen.begin(), seen.end(), id) == seen.end()) {
          seen.push_back(id);
          logical_ids.push_back(id);
        }
      }
  }

  // build ports: one "in" at k=0 on the diagonal, one "out" at k=n_k-1
  json ports = json::array();
  for (auto id : logical_ids) {
    // find the diagonal patch
    std::optional<std::pair<size_t,size_t>> in_pos;
    for (size_t d = 0; d < std::min(n_i,n_j); ++d) {
      if (auto* p = get_patch(d,d); p && p->occupied() && p->get_logical_id()==id) {
        in_pos = {d,d};
        break;
      }
    }
    // find *any* final patch
    std::optional<std::pair<size_t,size_t>> out_pos;
    for (size_t i = 0; i < n_i && !out_pos; ++i)
    for (size_t j = 0; j < n_j; ++j) {
      if (auto* p = get_patch(i,j); p && p->occupied() && p->get_logical_id()==id) {
        out_pos = {i,j};
        break;
      }
    }

    if (in_pos) {
      ports.push_back({
        {"i", in_pos->first},
        {"j", in_pos->second},
        {"k", 0},
        {"d","K"},
        {"e","-"},
        {"c",1}
      });
    }
    if (out_pos) {
      ports.push_back({
        {"i", out_pos->first},
        {"j", out_pos->second},
        {"k", n_k-1},
        {"d","K"},
        {"e","+"},
        {"c",1}
      });
    }
  }
  data["n_p"] = ports.size();
  data["ports"] = std::move(ports);

  //  build a map: patch_id -> logical qubit ID
  std::unordered_map<int,QubitIdType> patch_to_logical;
  for (size_t i = 0; i < n_i; ++i) {
    for (size_t j = 0; j < n_j; ++j) {
      if (auto* p = get_patch(i,j); p && p->occupied()) {
        int patch_id = p->get_id();        // or however you store the patch index
        patch_to_logical[patch_id] = p->get_logical_id();
      }
    }
  }

  //  build lid->out-port index exactly as before
  std::unordered_map<QubitIdType,int> lid_to_out_port;
  for (int p = 0; p < (int)data["n_p"]; ++p) {
    if (data["ports"][p]["e"] == "+") {
      auto i = size_t(data["ports"][p]["i"]),
          j = size_t(data["ports"][p]["j"]);
      auto* q = get_patch(i,j);
      if (q) lid_to_out_port[q->get_logical_id()] = p;
    }
  }

  //convert patch→logical ID
  json stabs = json::array();
  for (auto const* gate : get_gates()) {
    if (gate->get_operation_type() != LatticeSurgeryOpType::measure)
      continue;

    for (size_t qi = 0; qi < gate->get_qubits().size(); ++qi) {
      int patch_id = gate->get_qubits()[qi];      // your stored patch index
      auto  mt       = gate->get_measure_types()[qi];

      // translate to logical qubit ID
      auto it_lid = patch_to_logical.find(patch_id);
      if (it_lid == patch_to_logical.end()) continue;
      QubitIdType lid = it_lid->second;

      // find the corresponding "+" port
      auto it_p = lid_to_out_port.find(lid);
      if (it_p == lid_to_out_port.end()) continue;

      // build a zeroed boundary-condition vector
      json stab = json::array();
      for (size_t p = 0; p < data["n_p"]; ++p)
        stab.push_back({{"KI",0},{"KJ",0}});

      // and mark the right bit on that port
      int port_idx = it_p->second;
      if (mt == MeasureType::x)      stab[port_idx]["KI"] = 1;
      else /* z */                   stab[port_idx]["KJ"] = 1;

      stabs.push_back(std::move(stab));
    }
  }
  data["n_s"]   = stabs.size();
  data["stabs"] = std::move(stabs);


  // port_cubes = the open cube for each port
  json port_cubes = json::array();
  for (auto const& P : data["ports"]) {
    size_t i = P["i"], j = P["j"], k = P["k"];
    auto   d = P["d"].get<std::string>();
    auto   e = P["e"].get<std::string>();

    if (e == "-") {
      port_cubes.push_back({i,j,k});
    } else {
      if      (d=="I") port_cubes.push_back({i+1,j,k});
      else if (d=="J") port_cubes.push_back({i,j+1,k});
      else              port_cubes.push_back({i,j,k+1});
    }
  }
  data["port_cubes"] = std::move(port_cubes);

  // zero-init all of the SAT arrays
  data["ExistI"] = init_3d(n_i,n_j,n_k,false);
  data["ExistJ"] = init_3d(n_i,n_j,n_k,false);
  data["ExistK"] = init_3d(n_i,n_j,n_k,false);
  data["ColorI"] = init_3d(n_i,n_j,n_k,false);
  data["ColorJ"] = init_3d(n_i,n_j,n_k,false);
  data["ColorKP"] = init_3d(n_i,n_j,n_k,false);
  data["ColorKM"] = init_3d(n_i,n_j,n_k,false);
  data["NodeY"]  = init_3d(n_i,n_j,n_k,false);

  data["CorrIJ"] = init_4d(n_i,n_j,n_k,data["n_s"],false);
  data["CorrIK"] = init_4d(n_i,n_j,n_k,data["n_s"],false);
  data["CorrJI"] = init_4d(n_i,n_j,n_k,data["n_s"],false);
  data["CorrJK"] = init_4d(n_i,n_j,n_k,data["n_s"],false);
  data["CorrKI"] = init_4d(n_i,n_j,n_k,data["n_s"],false);
  data["CorrKJ"] = init_4d(n_i,n_j,n_k,data["n_s"],false);

  // force the bottom and top K-pipes (all your "ports") to exist
  for (auto const& P : data["ports"].get<json::array_t>()) {
    size_t i = P["i"], j = P["j"], k = P["k"];
    if (P["d"] == "K") {
      // bottom ports are at k=0 with e="-", top ports at k=n_k-1 with e="+"
      if( k == 0){
        data["ExistK"][i][j][k] = true;
      }
      else{
        data["ExistK"][i][j][gate_depth] = true;
      }
    }
  }

  // re-seed t=0 from your original occupied patches
  for(size_t x=0; x<n_i; x++){
    for(size_t k=0; k<gate_depth; k++){
      data["ExistK"][x][x][k] = true;
      // // Set initial color for K-pipes (alternating pattern)
      data["ColorKP"][x][x][k] = ((x + x + k) % 2 == 0);
      data["ColorKM"][x][x][k] = ((x + x + k) % 2 == 0);
    }
  }

  // append gate info to json
  std::vector<std::pair<size_t, size_t>> patch_pos(_qubits.size(), std::make_pair(0, 0));
  for(size_t x=0; x<get_grid_cols(); x++){
    for(size_t y=0; y<get_grid_rows(); y++){
      patch_pos[get_patch_id(x, y)].first = x;
      patch_pos[get_patch_id(x, y)].second = y;
    }
  }

  // First pass: Set up pipes and correlation surfaces
  for(auto gate: _gate_list){
    fmt::print("{}: {} ", gate->get_depth(), gate->get_type_str());
    for(auto patch: gate->get_qubits()){
      fmt::print( " ({},{}) ", patch_pos[patch].first, patch_pos[patch].second);
      
    }
    fmt::print("\n");
    if(gate->get_operation_type() == LatticeSurgeryOpType::measure && gate->get_num_qubits() == 1){ // discard patch
      auto patch = gate->get_qubits().front();
      for(auto d = gate->get_depth(); d < gate_depth; d++) data["ExistK"][patch_pos[patch].first][patch_pos[patch].second][d] = false;
    } 
    else if(gate->get_operation_type() == LatticeSurgeryOpType::measure){ // merge
      bool x_direction = patch_pos[gate->get_qubits()[0]].first == patch_pos[gate->get_qubits()[1]].first;
      if(x_direction){
        size_t max_y = 0;
        for(size_t i=0; i<gate->get_num_qubits(); i++){
          auto patch = gate->get_qubits()[i];
          if(max_y < patch_pos[patch].second) max_y = patch_pos[patch].second;
        }
        // Set up J-pipes for x-direction merge
        for(size_t i=0; i< gate->get_num_qubits(); i++){
          auto patch = gate->get_qubits()[i];
          if(max_y != patch_pos[patch].second) {
            data["ExistJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
            // Set initial color for J-pipe
            if(gate->get_measure_types()[i] == MeasureType::y){
              fmt::println("Y measurement at ({},{}) at time {}", patch_pos[patch].first, patch_pos[patch].second, gate->get_depth());
              data["ColorJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
                ((patch_pos[patch].first + patch_pos[patch].second + gate->get_depth()) % 2 == 0);
            }
            else{
              // For non-Y measurements, propagate color from previous timestep if it exists
              if(gate->get_depth() > 0) {
                data["ColorJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
                  data["ColorJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth() - 1];
              } else {
                data["ColorJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
                  ((patch_pos[patch].first + patch_pos[patch].second + gate->get_depth()) % 2 == 0);
              }
            }
          }
          // Set correlation surfaces for all patches in the merge
          for(auto d = gate->get_depth(); d < gate_depth; d++) {
            data["ExistK"][patch_pos[patch].first][patch_pos[patch].second][d] = true;
            // Set color for K-pipe (alternating pattern), but skip measurement time for Y measurements
            if(gate->get_measure_types()[i] != MeasureType::y || d != gate->get_depth()) {
              data["ColorKP"][patch_pos[patch].first][patch_pos[patch].second][d] = 
                ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
              data["ColorKM"][patch_pos[patch].first][patch_pos[patch].second][d] = 
                ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
            }
          }
          
          // For non-Y measurements, set correlation surfaces based on measurement type
          if(gate->get_measure_types()[i] != MeasureType::y) {
            data["CorrJI"][1][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
            data["CorrKJ"][1][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
          }
        }
      }
      else{
        size_t max_x = 0;
        for(size_t i=0; i<gate->get_num_qubits(); i++){
          auto patch = gate->get_qubits()[i];
          if(max_x < patch_pos[patch].first) max_x = patch_pos[patch].first;
        }
        // Set up I-pipes for y-direction merge
        for(size_t i=0; i< gate->get_num_qubits(); i++){
          auto patch = gate->get_qubits()[i];
          if(max_x != patch_pos[patch].first) {
            data["ExistI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
            // Set initial color for I-pipe
            if(gate->get_measure_types()[i] == MeasureType::y){
              fmt::println("Y measurement at ({},{}) at time {}", patch_pos[patch].first, patch_pos[patch].second, gate->get_depth());
              data["ColorI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
                ((patch_pos[patch].first + patch_pos[patch].second + gate->get_depth()) % 2 == 0);
            }
            else{
              // For non-Y measurements, propagate color from previous timestep if it exists
              if(gate->get_depth() > 0) {
                data["ColorI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
                  data["ColorI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth() - 1];
              } else {
                data["ColorI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
                  ((patch_pos[patch].first + patch_pos[patch].second + gate->get_depth()) % 2 == 0);
              }
            }
          }
          // Set correlation surfaces for all patches in the merge
          for(auto d = gate->get_depth(); d < gate_depth; d++) {
            data["ExistK"][patch_pos[patch].first][patch_pos[patch].second][d] = true;
            // Set color for K-pipe (alternating pattern), but skip measurement time for Y measurements
            if(gate->get_measure_types()[i] != MeasureType::y || d != gate->get_depth()) {
              data["ColorKP"][patch_pos[patch].first][patch_pos[patch].second][d] = 
                ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
              data["ColorKM"][patch_pos[patch].first][patch_pos[patch].second][d] = 
                ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
            }
          }
          
          // For non-Y measurements, set correlation surfaces based on measurement type
          if(gate->get_measure_types()[i] != MeasureType::y) {
            data["CorrIJ"][1][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
            data["CorrKI"][1][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
          }
        }
      }
    }
  }

  // Second pass: Handle Y measurements - create Hadamard edges only at L-corner (flagged by Y measurement)
  for (auto const* gate : get_gates()) {
    if (gate->get_operation_type() != LatticeSurgeryOpType::measure)
      continue;
    size_t t = gate->get_depth();
    for (size_t qi = 0; qi < gate->get_qubits().size(); ++qi) {
      auto patch_id = gate->get_qubits()[qi];
      auto [i,j] = _grid.get_patch_position(patch_id);
      if (gate->get_measure_types()[qi] == MeasureType::y) {
        // Only perform color switch at L-corner (Y measurement)
        fmt::println("L-corner Y measurement at ({},{}) at time {}: generating Hadamard color switch", i, j, t);
        // Ensure K-pipe exists at Y measurement time
        data["ExistK"][i][j][t] = true;
        // Set different colors for K-pipe minus and plus ends to create Hadamard edge
        data["ColorKM"][i][j][t] = ((i + j + t) % 2 == 0);  // Even pattern
        data["ColorKP"][i][j][t] = !data["ColorKM"][i][j][t];  // Opposite color
        // Set both correlation surfaces for Y-basis measurement
        data["CorrKI"][1][i][j][t] = true;
        data["CorrKJ"][1][i][j][t] = true;
        // For L-corner, also flip the color of the I or J pipe that enters the corner
        // Check if this is a vertical or horizontal L-corner
        // If there is an I-pipe from the left, flip its color at t+1
        if (i > 0 && data["ExistI"][i-1][j][t]) {
          data["ColorI"][i-1][j][t+1] = !data["ColorI"][i-1][j][t];
        }
        // If there is a J-pipe from below, flip its color at t+1
        if (j > 0 && data["ExistJ"][i][j-1][t]) {
          data["ColorJ"][i][j-1][t+1] = !data["ColorJ"][i][j-1][t];
        }
      }
    }
  }

  // Third pass: Propagate colors along contiguous pipes
  for(size_t k=0; k<n_k; k++) {
    // Propagate I-pipe colors
    for(size_t i=0; i<n_i-1; i++) {
      for(size_t j=0; j<n_j; j++) {
        if(data["ExistI"][i][j][k] && data["ExistI"][i+1][j][k]) {
          // Check if this is a Y measurement time and if either pipe is connected to a Y measurement
          bool is_y_measurement_time = false;
          for (auto const* gate : get_gates()) {
            if (gate->get_operation_type() == LatticeSurgeryOpType::measure && 
                gate->get_depth() == k) {
              for (size_t qi = 0; qi < gate->get_qubits().size(); ++qi) {
                if (gate->get_measure_types()[qi] == MeasureType::y) {
                  auto [yi, yj] = _grid.get_patch_position(gate->get_qubits()[qi]);
                  // Check if either pipe is connected to this Y measurement
                  if ((i == yi-1 && j == yj) || (i+1 == yi-1 && j == yj)) {
                    is_y_measurement_time = true;
                    break;
                  }
                }
              }
              if (is_y_measurement_time) break;
            }
          }
          
          // Only propagate if this is not a Y measurement time or if pipes are not connected to Y measurements
          if (!is_y_measurement_time && data["ColorI"][i][j][k] != data["ColorI"][i+1][j][k]) {
            data["ColorI"][i+1][j][k] = data["ColorI"][i][j][k];
          }
        }
      }
    }
    // Propagate J-pipe colors
    for(size_t i=0; i<n_i; i++) {
      for(size_t j=0; j<n_j-1; j++) {
        if(data["ExistJ"][i][j][k] && data["ExistJ"][i][j+1][k]) {
          // Check if this is a Y measurement time and if either pipe is connected to a Y measurement
          bool is_y_measurement_time = false;
          for (auto const* gate : get_gates()) {
            if (gate->get_operation_type() == LatticeSurgeryOpType::measure && 
                gate->get_depth() == k) {
              for (size_t qi = 0; qi < gate->get_qubits().size(); ++qi) {
                if (gate->get_measure_types()[qi] == MeasureType::y) {
                  auto [yi, yj] = _grid.get_patch_position(gate->get_qubits()[qi]);
                  // Check if either pipe is connected to this Y measurement
                  if ((i == yi && j == yj-1) || (i == yi && j+1 == yj-1)) {
                    is_y_measurement_time = true;
                    break;
                  }
                }
              }
              if (is_y_measurement_time) break;
            }
          }
          
          // Only propagate if this is not a Y measurement time or if pipes are not connected to Y measurements
          if (!is_y_measurement_time && data["ColorJ"][i][j][k] != data["ColorJ"][i][j+1][k]) {
            data["ColorJ"][i][j+1][k] = data["ColorJ"][i][j][k];
          }
        }
      }
    }
    // Propagate K-pipe colors
    for(size_t i=0; i<n_i; i++) {
      for(size_t j=0; j<n_j; j++) {
        if(data["ExistK"][i][j][k] && k+1 < n_k && data["ExistK"][i][j][k+1]) {
          // Ensure consistent colors along K-pipes
          // Only propagate if both ends have the same color (no Y measurement color difference)
          if(data["ColorKM"][i][j][k] == data["ColorKP"][i][j][k+1]) {
            // Colors are the same, so we can propagate normally
          } else {
            // Colors are different - this might be a Y measurement, so don't override
            // The color difference will create a Hadamard edge in ZX representation
          }
        }
      }
    }
  }

  // no extra optional stuff
  data["optional"] = json::object();

  // pretty-print
  return data.dump(2);
}
}