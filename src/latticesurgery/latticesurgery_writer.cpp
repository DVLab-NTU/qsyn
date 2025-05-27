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
static json init_3d(size_t ni, size_t nj, size_t nk, int def = 0) {
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

static json init_4d(size_t ni, size_t nj, size_t nk, size_t ns, int def = 0) {
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
  
  // n_k = gate_depth+1;
  
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

      // find the corresponding “+” port
      auto it_p = lid_to_out_port.find(lid);
      if (it_p == lid_to_out_port.end()) continue;

      // build a zeroed boundary‐condition vector
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

  // zero‐init all of the SAT arrays
  data["ExistI"] = init_3d(n_i,n_j,n_k,0);
  data["ExistJ"] = init_3d(n_i,n_j,n_k,0);
  data["ExistK"] = init_3d(n_i,n_j,n_k,0);
  data["ColorI"] = init_3d(n_i,n_j,n_k,0);
  data["ColorJ"] = init_3d(n_i,n_j,n_k,0);
  data["NodeY"]  = init_3d(n_i,n_j,n_k,0);

  data["CorrIJ"] = init_4d(n_i,n_j,n_k,data["n_s"],0);
  data["CorrIK"] = init_4d(n_i,n_j,n_k,data["n_s"],0);
  data["CorrJI"] = init_4d(n_i,n_j,n_k,data["n_s"],0);
  data["CorrJK"] = init_4d(n_i,n_j,n_k,data["n_s"],0);
  data["CorrKI"] = init_4d(n_i,n_j,n_k,data["n_s"],0);
  data["CorrKJ"] = init_4d(n_i,n_j,n_k,data["n_s"],0);

  // force the bottom and top K-pipes (all your “ports”) to exist
  for (auto const& P : data["ports"].get<json::array_t>()) {
    size_t i = P["i"], j = P["j"], k = P["k"];
    if (P["d"] == "K") {
      // bottom ports are at k=0 with e="-", top ports at k=n_k-1 with e="+"
      if( k == 0){
        data["ExistK"][i][j][k] = 1;
      }
      else{
        data["ExistK"][i][j][gate_depth] = 1;
      }
      
    }
  }

  // mark every measurement patch as NodeY at its gate time
  //    note: gate_times is your map<gate_id, time>
  for (auto const* gate : get_gates()) {
    if (gate->get_operation_type() != LatticeSurgeryOpType::measure)
      continue;
    size_t t = gate_times.at(gate->get_id());
    for (auto patch_id : gate->get_qubits()) {
      auto [i,j] = _grid.get_patch_position(patch_id);
      // data["NodeY"][i][j][t] = 1;
    }
  }

  // re‐seed t=0 from your original occupied patches
  // for (size_t i = 0; i < n_i; ++i) {
  //   for (size_t j = 0; j < n_j; ++j) {
  //     if (auto* p = get_patch(i,j); p && p->occupied()) {
  //       // always have a K pipe at time 0
  //       // data["ExistK"][i][j][0] = 1;
  //       // orientation → I‐ or J‐pipe
  //       // if (p->get_orientation())
  //       //   data["ExistI"][i][j][0] = 1;
  //       // else
  //       //   data["ExistJ"][i][j][0] = 1;
  //     }
  //   }
  // }
  for(size_t x=0; x<n_i; x++){
    for(size_t k=0; k<gate_depth; k++){
      data["ExistK"][x][x][k] = 1;
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

  for(auto gate: _gate_list){
    fmt::print("{}: {} ", gate->get_depth(), gate->get_type_str());
    for(auto patch: gate->get_qubits()){
      fmt::print( " ({},{}) ", patch_pos[patch].first, patch_pos[patch].second);
      
    }
    fmt::print("\n");
    if(gate->get_operation_type() == LatticeSurgeryOpType::measure && gate->get_num_qubits() == 1){ // discard patch
      auto patch = gate->get_qubits().front();
      data["ExistK"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 0;
    } 
    else if(gate->get_operation_type() == LatticeSurgeryOpType::measure){ // merge
      bool x_direction = patch_pos[gate->get_qubits()[0]].first == patch_pos[gate->get_qubits()[1]].first;
      if(x_direction){
        size_t max_y = 0;
        for(size_t i=0; i<gate->get_num_qubits(); i++){
          auto patch = gate->get_qubits()[i];
          if(max_y < patch_pos[patch].second) max_y = patch_pos[patch].second;
        }
        for(size_t i=0; i< gate->get_num_qubits(); i++){
          auto patch = gate->get_qubits()[i];
          if(max_y != patch_pos[patch].second) data["ExistJ"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 1;
          data["ExistK"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 1;
          data["CorrJI"][1][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 1;
          data["CorrKJ"][1][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 1;
        }
      }
      else{
        size_t max_x = 0;
        for(size_t i=0; i<gate->get_num_qubits(); i++){
          auto patch = gate->get_qubits()[i];
          if(max_x < patch_pos[patch].first) max_x = patch_pos[patch].first;
        }
        for(size_t i=0; i< gate->get_num_qubits(); i++){
          auto patch = gate->get_qubits()[i];
          if(max_x != patch_pos[patch].first) data["ExistI"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 1;
          data["ExistK"][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 1;
          data["CorrIJ"][1][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 1;
          data["CorrKI"][1][patch_pos[patch].first][patch_pos[patch].second][gate->get_depth()] = 1;
        }
      }
    }
  }


  // no extra optional stuff
  data["optional"] = json::object();

  // pretty‐print
  return data.dump(2);
}
}