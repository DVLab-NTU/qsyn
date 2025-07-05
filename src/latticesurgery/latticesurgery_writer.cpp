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
    else if(gate->get_operation_type() == LatticeSurgeryOpType::hadamard_l) { // L-pipe hadamard
      auto patches = gate->get_qubits();
      if (patches.size() >= 3) {
        // patches[0] = start, patches[1] = corner, patches[2+] = destinations
        auto start_pos = patch_pos[patches[0]];
        auto corner_pos = patch_pos[patches[1]];
        
        // Create L-pipe with continuous color
        // Determine if L-pipe goes horizontally first then vertically, or vice versa
        bool horizontal_first = (start_pos.first != corner_pos.first);
        
        if (horizontal_first) {
          // Start -> Corner: I-direction (horizontal)
          size_t start_i = std::min(start_pos.first, corner_pos.first);
          size_t end_i = std::max(start_pos.first, corner_pos.first);
          for (size_t i = start_i; i < end_i; i++) {
            data["ExistI"][i][start_pos.second][gate->get_depth()] = true;
            // Set continuous color for I-pipe based on target destination
            data["ColorI"][i][start_pos.second][gate->get_depth()] = true; // Set to target color
          }
          
          // Corner -> Destinations: J-direction (vertical)
          for (size_t dest_idx = 2; dest_idx < patches.size(); dest_idx++) {
            auto dest_pos = patch_pos[patches[dest_idx]];
            size_t start_j = std::min(corner_pos.second, dest_pos.second);
            size_t end_j = std::max(corner_pos.second, dest_pos.second);
            for (size_t j = start_j; j < end_j; j++) {
              data["ExistJ"][corner_pos.first][j][gate->get_depth()] = true;
              // Set continuous color for J-pipe - flip color at corner for hadamard
              data["ColorJ"][corner_pos.first][j][gate->get_depth()] = false; // Opposite of I-pipe color
            }
          }
        } else {
          // Start -> Corner: J-direction (vertical)
          size_t start_j = std::min(start_pos.second, corner_pos.second);
          size_t end_j = std::max(start_pos.second, corner_pos.second);
          for (size_t j = start_j; j < end_j; j++) {
            data["ExistJ"][start_pos.first][j][gate->get_depth()] = true;
            // Set continuous color for J-pipe based on target destination
            data["ColorJ"][start_pos.first][j][gate->get_depth()] = true; // Set to target color
          }
          
          // Corner -> Destinations: I-direction (horizontal)
          for (size_t dest_idx = 2; dest_idx < patches.size(); dest_idx++) {
            auto dest_pos = patch_pos[patches[dest_idx]];
            size_t start_i = std::min(corner_pos.first, dest_pos.first);
            size_t end_i = std::max(corner_pos.first, dest_pos.first);
            for (size_t i = start_i; i < end_i; i++) {
              data["ExistI"][i][corner_pos.second][gate->get_depth()] = true;
              // Set continuous color for I-pipe - flip color at corner for hadamard
              data["ColorI"][i][corner_pos.second][gate->get_depth()] = false; // Opposite of J-pipe color
            }
          }
        }
        
        // Set K-pipes for all involved patches
        for (auto patch : patches) {
          for(auto d = gate->get_depth(); d < gate_depth; d++) {
            data["ExistK"][patch_pos[patch].first][patch_pos[patch].second][d] = true;
            // Set color for K-pipe with automatic color switch at corner for hadamard
            if (patch == patches[1]) { // corner patch
              // Create hadamard by setting different colors for K-pipe ends
              data["ColorKP"][patch_pos[patch].first][patch_pos[patch].second][d] = 
                ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
              data["ColorKM"][patch_pos[patch].first][patch_pos[patch].second][d] = 
                !data["ColorKP"][patch_pos[patch].first][patch_pos[patch].second][d]; // Opposite color
            } else {
              // Normal K-pipe coloring for non-corner patches
              data["ColorKP"][patch_pos[patch].first][patch_pos[patch].second][d] = 
                ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
              data["ColorKM"][patch_pos[patch].first][patch_pos[patch].second][d] = 
                ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
            }
          }
        }
      }
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
            // Set initial color for J-pipe (removed Y measurement logic)
            // if(gate->get_depth() > 0) {
            //   data["ColorJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
            //     data["ColorJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth() - 1];
            // } else {
            //   data["ColorJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
            //     ((patch_pos[patch].first + patch_pos[patch].second + gate->get_depth()) % 2 == 0);
            // }
          }
          // Set correlation surfaces for all patches in the merge
          for(auto d = gate->get_depth(); d < gate_depth; d++) {
            data["ExistK"][patch_pos[patch].first][patch_pos[patch].second][d] = true;
            // Set color for K-pipe (normal coloring, no Y measurement handling)
            data["ColorKP"][patch_pos[patch].first][patch_pos[patch].second][d] = 
              ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
            data["ColorKM"][patch_pos[patch].first][patch_pos[patch].second][d] = 
              ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
          }
          
          // Set correlation surfaces based on measurement type (removed Y measurement special case)
          if (i < gate->get_measure_types().size()) {
            // Use stabilizer index 0 instead of hardcoded 1
            if (data["n_s"].get<size_t>() > 0) {
              data["CorrJI"][0][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
              data["CorrKJ"][0][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
            }
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
            // Set initial color for I-pipe (removed Y measurement logic)
            if(gate->get_depth() > 0) {
              data["ColorI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
                data["ColorI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth() - 1];
            } else {
              data["ColorI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
                ((patch_pos[patch].first + patch_pos[patch].second + gate->get_depth()) % 2 == 0);
            }
          }
          // Set correlation surfaces for all patches in the merge
          for(auto d = gate->get_depth(); d < gate_depth; d++) {
            data["ExistK"][patch_pos[patch].first][patch_pos[patch].second][d] = true;
            // Set color for K-pipe (normal coloring, no Y measurement handling)
            data["ColorKP"][patch_pos[patch].first][patch_pos[patch].second][d] = 
              ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
            data["ColorKM"][patch_pos[patch].first][patch_pos[patch].second][d] = 
              ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
          }
          
          // Set correlation surfaces based on measurement type (removed Y measurement special case)
          if (i < gate->get_measure_types().size()) {
            // Use stabilizer index 0 instead of hardcoded 1
            if (data["n_s"].get<size_t>() > 0) {
              data["CorrIJ"][0][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
              data["CorrKI"][0][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
            }
          }
        }
      }
    }
    else if(gate->get_operation_type() == LatticeSurgeryOpType::measure_c){ // merge
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
            data["ColorJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
            // Set initial color for J-pipe (removed Y measurement logic)
            // if(gate->get_depth() > 0) {
            //   data["ColorJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
            //     data["ColorJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth() - 1];
            // } else {
            //   data["ColorJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
            //     ((patch_pos[patch].first + patch_pos[patch].second + gate->get_depth()) % 2 == 0);
            // }
          }
          // Set correlation surfaces for all patches in the merge
          for(auto d = gate->get_depth(); d < gate_depth; d++) {
            data["ExistK"][patch_pos[patch].first][patch_pos[patch].second][d] = true;
            // Set color for K-pipe (normal coloring, no Y measurement handling)
            data["ColorKP"][patch_pos[patch].first][patch_pos[patch].second][d] = 
              ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
            data["ColorKM"][patch_pos[patch].first][patch_pos[patch].second][d] = 
              ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
          }
          
          // Set correlation surfaces based on measurement type (removed Y measurement special case)
          if (i < gate->get_measure_types().size()) {
            // Use stabilizer index 0 instead of hardcoded 1
            if (data["n_s"].get<size_t>() > 0) {
              data["CorrJI"][0][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
              data["CorrKJ"][0][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
            }
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
            data["ColorI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
            // Set initial color for I-pipe (removed Y measurement logic)
            // if(gate->get_depth() > 0) {
            //   data["ColorI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
            //     data["ColorI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth() - 1];
            // } else {
            //   data["ColorI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 
            //     ((patch_pos[patch].first + patch_pos[patch].second + gate->get_depth()) % 2 == 0);
            // }
          }
          // Set correlation surfaces for all patches in the merge
          for(auto d = gate->get_depth(); d < gate_depth; d++) {
            data["ExistK"][patch_pos[patch].first][patch_pos[patch].second][d] = true;
            // Set color for K-pipe (normal coloring, no Y measurement handling)
            data["ColorKP"][patch_pos[patch].first][patch_pos[patch].second][d] = 
              ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
            data["ColorKM"][patch_pos[patch].first][patch_pos[patch].second][d] = 
              ((patch_pos[patch].first + patch_pos[patch].second + d) % 2 == 0);
          }
          
          // Set correlation surfaces based on measurement type (removed Y measurement special case)
          if (i < gate->get_measure_types().size()) {
            // Use stabilizer index 0 instead of hardcoded 1
            if (data["n_s"].get<size_t>() > 0) {
              data["CorrIJ"][0][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
              data["CorrKI"][0][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = true;
            }
          }
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
          // Propagate color if pipes are contiguous and not part of L-pipe hadamard
          if (data["ColorI"][i][j][k] != data["ColorI"][i+1][j][k]) {
            data["ColorI"][i+1][j][k] = data["ColorI"][i][j][k];
          }
        }
      }
    }
    // Propagate J-pipe colors
    for(size_t i=0; i<n_i; i++) {
      for(size_t j=0; j<n_j-1; j++) {
        if(data["ExistJ"][i][j][k] && data["ExistJ"][i][j+1][k]) {
          // Propagate color if pipes are contiguous and not part of L-pipe hadamard
          if (data["ColorJ"][i][j][k] != data["ColorJ"][i][j+1][k]) {
            data["ColorJ"][i][j+1][k] = data["ColorJ"][i][j][k];
          }
        }
      }
    }
    // Propagate K-pipe colors
    for(size_t i=0; i<n_i; i++) {
      for(size_t j=0; j<n_j; j++) {
        if(data["ExistK"][i][j][k] && k+1 < n_k && data["ExistK"][i][j][k+1]) {
          // For K-pipes, maintain consistency unless there's a hadamard color switch
          if(data["ColorKM"][i][j][k] == data["ColorKP"][i][j][k]) {
            // Normal case: propagate same color
            data["ColorKM"][i][j][k+1] = data["ColorKM"][i][j][k];
            data["ColorKP"][i][j][k+1] = data["ColorKP"][i][j][k];
          }
          // If colors are different, it's a hadamard edge - don't change
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